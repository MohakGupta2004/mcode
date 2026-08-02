#include "commander.h"
#include "../providers/openrouter_provider.h"
#include "../providers/ollama_provider.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Color palette (ANSI SGR codes). Kept as plain escape strings rather than a
// theming system since this is a single small CLI, not a library.
// ---------------------------------------------------------------------------
namespace Color {
constexpr const char* Reset = "\x1b[0m";
constexpr const char* Bold = "\x1b[1m";
constexpr const char* Dim = "\x1b[2m";
constexpr const char* Red = "\x1b[31m";
constexpr const char* Green = "\x1b[32m";
constexpr const char* Yellow = "\x1b[33m";
constexpr const char* Blue = "\x1b[34m";
constexpr const char* Magenta = "\x1b[35m";
constexpr const char* Cyan = "\x1b[36m";
constexpr const char* Gray = "\x1b[90m";
constexpr const char* BoldGreen = "\x1b[1;32m";
constexpr const char* BoldCyan = "\x1b[1;36m";
constexpr const char* BoldRed = "\x1b[1;31m";
} // namespace Color

// ---------------------------------------------------------------------------
// Raw terminal helpers
// ---------------------------------------------------------------------------
namespace {

// RAII guard: put the terminal in raw mode (no echo, no line buffering) for the
// lifetime of the object and restore the previous settings on destruction, even
// if an exception unwinds through us. Optionally also turns on the terminal's
// bracketed-paste mode, which wraps a pasted block in ESC[200~ ... ESC[201~ so
// the reader can tell "user pasted text containing newlines" apart from "user
// pressed Enter N times" - without it every embedded newline in a paste looks
// like a real Enter and submits the line early.
class RawMode {
public:
  explicit RawMode(bool bracketedPaste = false)
      : ok_(false), pasteEnabled_(false) {
    if (tcgetattr(STDIN_FILENO, &orig_) == -1) {
      return;
    }
    termios raw = orig_;
    // ISIG must go too: without it, a real terminal turns Ctrl-C into SIGINT
    // (default-terminating the whole program) before it ever reaches readKey,
    // so the Key::CtrlC handling below - which just clears the current line -
    // never actually runs.
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
      return;
    }
    ok_ = true;
    if (bracketedPaste) {
      std::cout << "\x1b[?2004h" << std::flush;
      pasteEnabled_ = true;
    }
  }
  ~RawMode() {
    if (pasteEnabled_) {
      std::cout << "\x1b[?2004l" << std::flush;
    }
    if (ok_) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_);
    }
  }
  bool ok() const { return ok_; }

private:
  termios orig_{};
  bool ok_;
  bool pasteEnabled_;
};

enum class Key {
  Char,
  Enter,
  Backspace,
  AltBackspace,
  Up,
  Down,
  Left,
  Right,
  Escape,
  Tab,
  CtrlC,
  CtrlD,
  PasteStart,
  PasteEnd,
  Unknown,
};

// Read one logical keypress, decoding ANSI escape/CSI sequences: arrow keys
// (ESC[A/B/C/D) and bracketed-paste markers (ESC[200~ start, ESC[201~ end).
Key readKey(char& out) {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1) {
    return Key::CtrlD;
  }
  switch (c) {
  case '\r':
  case '\n':
    return Key::Enter;
  case 127:
  case 8:
    return Key::Backspace;
  case '\t':
    return Key::Tab;
  case 3:
    return Key::CtrlC;
  case 4:
    return Key::CtrlD;
  case 27: { // ESC or CSI sequence
    char seq0;
    if (read(STDIN_FILENO, &seq0, 1) != 1) {
      return Key::Escape;
    }
    if (seq0 == 'O') {
      char c2;
      if (read(STDIN_FILENO, &c2, 1) != 1) {
        return Key::Escape;
      }
      switch (c2) {
      case 'A': return Key::Up;
      case 'B': return Key::Down;
      case 'C': return Key::Right;
      case 'D': return Key::Left;
      default: return Key::Unknown;
      }
    }
    // macOS Terminal/iTerm send ESC followed by DEL (or BS) for Option-Backspace
    // ("delete word left") when Option is bound as the Meta key - the default
    // on both apps' built-in profiles.
    if (seq0 == 127 || seq0 == 8) {
      return Key::AltBackspace;
    }
    if (seq0 != '[') {
      out = seq0;
      return Key::Escape;
    }
    // CSI sequence: collect numeric parameter bytes up to the final byte.
    std::string params;
    char fin;
    while (true) {
      if (read(STDIN_FILENO, &fin, 1) != 1) {
        return Key::Escape;
      }
      if (fin >= '0' && fin <= '9') {
        params += fin;
        continue;
      }
      break;
    }
    if (fin == '~') {
      if (params == "200") return Key::PasteStart;
      if (params == "201") return Key::PasteEnd;
      return Key::Unknown;
    }
    switch (fin) {
    case 'A': return Key::Up;
    case 'B': return Key::Down;
    case 'C': return Key::Right;
    case 'D': return Key::Left;
    default: return Key::Unknown;
    }
  }
  default:
    out = c;
    return Key::Char;
  }
}

// Consume raw bytes up to and including the ESC[201~ bracketed-paste end
// marker, returning everything in between. Embedded CR/LF bytes are folded to
// a single space so the pasted text stays a single logical line (the input
// buffer/redraw model here doesn't support multi-line editing).
std::string readPastedText() {
  std::string pasted;
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\r' || c == '\n') {
      if (pasted.empty() || pasted.back() != ' ') {
        pasted += ' ';
      }
      continue;
    }
    if (c == 27) {
      char seq0;
      if (read(STDIN_FILENO, &seq0, 1) == 1 && seq0 == '[') {
        std::string params;
        char fin;
        bool matchedEnd = false;
        while (read(STDIN_FILENO, &fin, 1) == 1) {
          if (fin >= '0' && fin <= '9') {
            params += fin;
            continue;
          }
          matchedEnd = (fin == '~' && params == "201");
          break;
        }
        if (matchedEnd) {
          break;
        }
        // Not the end marker - drop it rather than corrupt the pasted text.
        continue;
      }
      continue;
    }
    pasted += c;
  }
  return pasted;
}

struct Entry {
  std::string name;
  bool is_dir;
};

// List `dir`, optionally keeping only entries whose name starts with `filter`
// (case-insensitive). Directories sort first, then alphabetical.
std::vector<Entry> listDir(const fs::path& dir, const std::string& filter) {
  std::vector<Entry> entries;
  std::string lf = filter;
  std::transform(lf.begin(), lf.end(), lf.begin(), ::tolower);
  try {
    for (const auto& e : fs::directory_iterator(dir)) {
      std::string name = e.path().filename().string();
      if (!lf.empty()) {
        std::string ln = name;
        std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
        if (ln.rfind(lf, 0) != 0) {
          continue;
        }
      }
      entries.push_back({name, e.is_directory()});
    }
  } catch (const fs::filesystem_error&) {
    // Unreadable directory -> empty list.
  }
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.is_dir != b.is_dir) {
      return a.is_dir > b.is_dir;
    }
    return a.name < b.name;
  });
  return entries;
}

// Interactive directory picker. Returns the chosen relative path (files only;
// directories can be entered), or "" if the user cancelled with Esc.
std::string filePicker() {
  const int kWindow = 10; // max visible rows
  std::string base;       // relative dir prefix, e.g. "src/"
  std::string filter;
  int selected = 0;
  int prevLines = 0;

  std::cout << "\x1b[?25l"; // hide cursor

  auto clearMenu = [&]() {
    if (prevLines > 0) {
      std::cout << "\x1b[" << prevLines << "A"; // up N lines
    }
    std::cout << "\r\x1b[J"; // clear from cursor to end of screen
  };

  // Move `base` one directory up. If we are inside a descended path, drop the
  // last segment; otherwise (at or above cwd) prepend another "../".
  auto goUp = [&]() {
    bool inUpMode = base.size() >= 3 && base.compare(base.size() - 3, 3, "../") == 0;
    if (!base.empty() && !inUpMode) {
      base.pop_back(); // trailing slash
      auto pos = base.find_last_of('/');
      base = (pos == std::string::npos) ? "" : base.substr(0, pos + 1);
    } else {
      base += "../";
    }
  };

  std::string result;
  bool done = false;
  while (!done) {
    fs::path dir = base.empty() ? fs::path(".") : fs::path(base);
    std::vector<Entry> entries = listDir(dir, filter);
    if (selected >= static_cast<int>(entries.size())) {
      selected = entries.empty() ? 0 : static_cast<int>(entries.size()) - 1;
    }
    if (selected < 0) {
      selected = 0;
    }

    clearMenu();

    std::string header = std::string("  ") + Color::Cyan + "@" + base + Color::Bold +
                         Color::Cyan + filter + Color::Reset + Color::Dim +
                         "  (↑↓ move · → open · ⏎/space pick · esc cancel)" +
                         Color::Reset + "\r\n";
    std::cout << header;
    int lines = 1;

    int first = 0;
    if (selected >= kWindow) {
      first = selected - kWindow + 1;
    }
    int last = std::min(static_cast<int>(entries.size()), first + kWindow);
    for (int i = first; i < last; ++i) {
      const Entry& e = entries[i];
      std::string name = e.is_dir ? (std::string(Color::Blue) + Color::Bold + e.name + "/" + Color::Reset)
                                   : e.name;
      std::string label = "  " + name;
      if (i == selected) {
        std::cout << "\x1b[7m  " << (e.is_dir ? e.name + "/" : e.name) << "\x1b[0m\r\n"; // reverse video
      } else {
        std::cout << label << "\r\n";
      }
      ++lines;
    }
    if (entries.empty()) {
      std::cout << "  " << Color::Yellow << "(no matches)" << Color::Reset << "\r\n";
      ++lines;
    }
    prevLines = lines;
    std::cout.flush();

    char ch = 0;
    Key k = readKey(ch);
    switch (k) {
    case Key::Up:
      if (selected > 0) {
        --selected;
      }
      break;
    case Key::Down:
      if (selected + 1 < static_cast<int>(entries.size())) {
        ++selected;
      }
      break;
    case Key::Enter:
      // Commit current highlight (dir or file) and return to the prompt.
      if (!entries.empty()) {
        const Entry& e = entries[selected];
        result = base + e.name + (e.is_dir ? "/" : "");
        done = true;
      }
      break;
    case Key::Right:
    case Key::Tab:
      // Navigate: descend into a dir, or pick a file.
      if (!entries.empty()) {
        const Entry& e = entries[selected];
        if (e.is_dir) {
          base += e.name + "/"; // descend
          filter.clear();
          selected = 0;
        } else {
          result = base + e.name; // pick file
          done = true;
        }
      }
      break;
    case Key::Left:
      if (!base.empty()) {
        // strip trailing slash then last component
        base.pop_back();
        auto pos = base.find_last_of('/');
        base = (pos == std::string::npos) ? "" : base.substr(0, pos + 1);
        filter.clear();
        selected = 0;
      }
      break;
    case Key::Backspace:
      if (!filter.empty()) {
        filter.pop_back();
        selected = 0;
      } else if (!base.empty()) {
        base.pop_back();
        auto pos = base.find_last_of('/');
        base = (pos == std::string::npos) ? "" : base.substr(0, pos + 1);
        selected = 0;
      }
      break;
    case Key::Char:
      if (ch == ' ') {
        // Space commits current highlight (dir or file) and returns to prompt.
        if (!entries.empty()) {
          const Entry& e = entries[selected];
          result = base + e.name + (e.is_dir ? "/" : "");
          done = true;
        }
      } else if (ch == '/') {
        if (filter == "..") {
          // Typing "../" navigates to the parent directory (directory_iterator
          // never yields "..", so it can never appear as a normal match).
          goUp();
          filter.clear();
          selected = 0;
        } else if (!entries.empty() && entries[selected].is_dir) {
          // treat like descending into highlighted dir
          base += entries[selected].name + "/";
          filter.clear();
          selected = 0;
        }
      } else {
        filter += ch;
        selected = 0;
      }
      break;
    case Key::Escape:
    case Key::CtrlC:
    case Key::CtrlD:
      done = true; // cancel, result stays empty
      break;
    default:
      break;
    }
  }

  clearMenu();
  std::cout << "\x1b[?25h"; // show cursor
  std::cout.flush();
  return result;
}

// Interactive model chooser. Renders a scrolling, type-to-filter list of model
// ids and lets the user move with the arrow keys and commit with Enter. Returns
// the chosen model id, or "" if the user cancels (esc / ctrl-c / ctrl-d).
//
// Unlike filePicker this owns its own RawMode guard, because it is invoked from
// command handling where the terminal is back in cooked mode.
std::string modelPicker(const std::vector<std::string>& models,
                        const std::string& current) {
  RawMode raw;
  if (!raw.ok()) {
    return ""; // not a tty; caller falls back to `/model <name>`
  }

  const int kWindow = 12; // max visible rows
  std::string filter;
  int selected = 0;
  int prevLines = 0;

  auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
  };

  std::cout << "\x1b[?25l"; // hide cursor

  auto clearMenu = [&]() {
    if (prevLines > 0) {
      std::cout << "\x1b[" << prevLines << "A"; // up N lines
    }
    std::cout << "\r\x1b[J"; // clear from cursor to end of screen
  };

  std::string result;
  bool done = false;
  while (!done) {
    // Build the filtered view fresh each frame so typing narrows the list.
    std::string needle = lower(filter);
    std::vector<const std::string*> view;
    for (const std::string& m : models) {
      if (needle.empty() || lower(m).find(needle) != std::string::npos) {
        view.push_back(&m);
      }
    }
    if (selected >= static_cast<int>(view.size())) {
      selected = view.empty() ? 0 : static_cast<int>(view.size()) - 1;
    }
    if (selected < 0) {
      selected = 0;
    }

    clearMenu();

    std::cout << "  " << Color::Dim << "model " << Color::Reset << Color::Bold
              << Color::Cyan << filter << Color::Reset << Color::Dim
              << "  (↑↓ move · ⏎ pick · esc cancel)" << Color::Reset << "\r\n";
    int lines = 1;

    int first = 0;
    if (selected >= kWindow) {
      first = selected - kWindow + 1;
    }
    int last = std::min(static_cast<int>(view.size()), first + kWindow);
    for (int i = first; i < last; ++i) {
      const std::string& m = *view[i];
      bool isCurrent = (m == current);
      std::string marker = isCurrent ? (std::string(Color::Green) + "* " + Color::Reset) : "  ";
      std::string label = std::string("  ") + marker + m;
      if (i == selected) {
        std::cout << "\x1b[7m  " << (isCurrent ? "* " : "  ") << m << "\x1b[0m\r\n"; // reverse video
      } else if (isCurrent) {
        std::cout << "  " << Color::BoldGreen << "* " << m << Color::Reset << "\r\n";
      } else {
        std::cout << label << "\r\n";
      }
      ++lines;
    }
    if (view.empty()) {
      std::cout << "  " << Color::Yellow << "(no matches)" << Color::Reset << "\r\n";
      ++lines;
    }
    prevLines = lines;
    std::cout.flush();

    char ch = 0;
    Key k = readKey(ch);
    switch (k) {
    case Key::Up:
      if (selected > 0) {
        --selected;
      }
      break;
    case Key::Down:
      if (selected + 1 < static_cast<int>(view.size())) {
        ++selected;
      }
      break;
    case Key::Enter:
      if (!view.empty()) {
        result = *view[selected];
        done = true;
      }
      break;
    case Key::Backspace:
      if (!filter.empty()) {
        filter.pop_back();
        selected = 0;
      }
      break;
    case Key::Char:
      filter += ch;
      selected = 0;
      break;
    case Key::Escape:
    case Key::CtrlC:
    case Key::CtrlD:
      done = true; // cancel, result stays empty
      break;
    default:
      break;
    }
  }

  clearMenu();
  std::cout << "\x1b[?25h"; // show cursor
  std::cout.flush();
  return result;
}

// Interactive provider chooser. Same shape as modelPicker but over the fixed,
// small gateway list ("openrouter", "ollama") rather than a fetched one.
std::string providerPicker(const std::vector<std::string>& providers,
                           const std::string& current) {
  RawMode raw;
  if (!raw.ok()) {
    return "";
  }

  int selected = 0;
  for (size_t i = 0; i < providers.size(); ++i) {
    if (providers[i] == current) {
      selected = static_cast<int>(i);
    }
  }
  int prevLines = 0;

  std::cout << "\x1b[?25l"; // hide cursor

  auto clearMenu = [&]() {
    if (prevLines > 0) {
      std::cout << "\x1b[" << prevLines << "A";
    }
    std::cout << "\r\x1b[J";
  };

  std::string result;
  bool done = false;
  while (!done) {
    clearMenu();

    std::cout << "  " << Color::Dim << "provider  (↑↓ move · ⏎ pick · esc cancel)"
              << Color::Reset << "\r\n";
    int lines = 1;

    for (size_t i = 0; i < providers.size(); ++i) {
      bool isCurrent = (providers[i] == current);
      std::string label = std::string("  ") + (isCurrent ? "* " : "  ") + providers[i];
      if (static_cast<int>(i) == selected) {
        std::cout << "\x1b[7m" << label << "\x1b[0m\r\n";
      } else if (isCurrent) {
        std::cout << "  " << Color::BoldGreen << "* " << providers[i] << Color::Reset << "\r\n";
      } else {
        std::cout << label << "\r\n";
      }
      ++lines;
    }
    prevLines = lines;
    std::cout.flush();

    char ch = 0;
    Key k = readKey(ch);
    switch (k) {
    case Key::Up:
      if (selected > 0) {
        --selected;
      }
      break;
    case Key::Down:
      if (selected + 1 < static_cast<int>(providers.size())) {
        ++selected;
      }
      break;
    case Key::Enter:
      result = providers[selected];
      done = true;
      break;
    case Key::Escape:
    case Key::CtrlC:
    case Key::CtrlD:
      done = true; // cancel, result stays empty
      break;
    default:
      break;
    }
  }

  clearMenu();
  std::cout << "\x1b[?25h"; // show cursor
  std::cout.flush();
  return result;
}

// Every command Commander::handle (or main's "exit" check) recognizes. Drives
// both the inline ghost suggestion shown while typing and the auto-expand on
// Enter for slash commands.
const std::vector<std::string> kKnownCommands = {
    "exit", "history", "clear", "save", "/provider", "/model",
};

// If `buf` is a non-empty, strict prefix of exactly one known command, return
// that command; otherwise "" (no match, or too ambiguous to guess).
std::string findUniqueSuggestion(const std::string& buf) {
  if (buf.empty()) {
    return "";
  }
  std::string match;
  int matches = 0;
  for (const auto& cmd : kKnownCommands) {
    if (cmd.size() > buf.size() && cmd.compare(0, buf.size(), buf) == 0) {
      match = cmd;
      ++matches;
    }
  }
  return matches == 1 ? match : "";
}

// Current terminal width in columns, falling back to 80 if it can't be read
// (e.g. output redirected to a file).
int termWidth() {
  struct winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    return ws.ws_col;
  }
  return 80;
}

// Redraw the input line: prompt + buffer + (optionally) a dim inline
// suggestion for the one command `buf` could be completing, then place the
// terminal cursor at `cursor` within the buffer (never inside the
// suggestion - it's a preview, not part of the line).
//
// `prevVisibleLen` is the on-screen character count (prompt+buf+ghost) from
// the previous call, threaded through by the caller across a whole readLine()
// session. It lets us find how many terminal rows the previous render wrapped
// across, so a long line that spills onto a second row gets fully erased
// before redrawing - a plain "\r\x1b[K" only clears the row the cursor is
// currently on, leaving stale characters on the row(s) above it once the
// buffer shrinks or the cursor moves back across a wrap boundary.
void redrawLine(const std::string& prompt, const std::string& buf, size_t cursor,
                size_t& prevVisibleLen) {
  int width = termWidth();
  // findUniqueSuggestion returns the whole matching command; only the part
  // beyond what's already typed should render as the ghost.
  std::string suggestion = (cursor == buf.size()) ? findUniqueSuggestion(buf) : "";
  std::string ghost = suggestion.empty() ? "" : suggestion.substr(buf.size());

  size_t prevRows = (width > 0 && prevVisibleLen > 0) ? (prevVisibleLen - 1) / width : 0;
  if (prevRows > 0) {
    std::cout << "\x1b[" << prevRows << "A"; // up to the first row we rendered
  }
  std::cout << "\r\x1b[J"; // clear that row and everything below it

  std::cout << Color::BoldCyan << prompt << Color::Reset << buf;
  if (!ghost.empty()) {
    std::cout << Color::Gray << ghost << Color::Reset;
  }

  size_t visibleLen = prompt.size() + buf.size() + ghost.size();
  size_t cursorPos = prompt.size() + cursor;
  size_t afterPrintRow = width > 0 ? visibleLen / width : 0;
  size_t targetRow = width > 0 ? cursorPos / width : 0;
  size_t targetCol = width > 0 ? cursorPos % width : cursorPos;
  if (afterPrintRow > targetRow) {
    std::cout << "\x1b[" << (afterPrintRow - targetRow) << "A";
  }
  std::cout << "\r";
  if (targetCol > 0) {
    std::cout << "\x1b[" << targetCol << "C";
  }

  prevVisibleLen = visibleLen;
  std::cout.flush();
}

} // namespace

// ---------------------------------------------------------------------------
// Public interactive reader
// ---------------------------------------------------------------------------
std::string Commander::readLine(const std::string& prompt, bool& eof) {
  eof = false;
  // Reset history browsing for this call: start past the newest entry (i.e.
  // showing a fresh draft), same as a shell resetting its cursor after Enter.
  historyIndex_ = history_.size();
  draft_.clear();

  RawMode raw(/*bracketedPaste=*/true);
  if (!raw.ok()) {
    // Not a tty (e.g. piped input) -> fall back to plain line reading.
    std::cout << Color::BoldCyan << prompt << Color::Reset;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {
      eof = true;
    }
    return line;
  }

  std::string buf;
  size_t cursor = 0;
  // On-screen length (prompt+buf+ghost) from the last redraw, so redrawLine
  // can tell whether the previous render wrapped onto extra terminal rows
  // and needs to clear them too. Reset to 0 here since nothing's drawn yet.
  size_t visibleLen = 0;
  redrawLine(prompt, buf, cursor, visibleLen);

  while (true) {
    char ch = 0;
    Key k = readKey(ch);
    switch (k) {
    case Key::Enter: {
      // Auto-complete an unambiguous slash command on submit (e.g. "/p" ->
      // "/provider"). Restricted to '/' input: a leading slash is an
      // unambiguous "this is a command" signal, whereas guessing at a bare
      // word (e.g. turning a one-letter chat message into "clear") could
      // silently swallow real chat text the user meant to send as-is.
      if (!buf.empty() && buf[0] == '/') {
        std::string suggestion = findUniqueSuggestion(buf);
        if (!suggestion.empty()) {
          buf = suggestion;
          std::cout << Color::Dim << " -> " << Color::Reset << Color::Bold
                    << buf << Color::Reset;
        }
      }
      std::cout << "\r\n";
      std::cout.flush();
      // Remember this line for Up/Down recall, skipping blanks and immediate
      // repeats so hammering Enter or Up doesn't pile up duplicate entries.
      if (!buf.empty() && (history_.empty() || history_.back() != buf)) {
        history_.push_back(buf);
      }
      return buf;
    }
    case Key::CtrlD:
      if (buf.empty()) {
        eof = true;
        std::cout << "\r\n";
        std::cout.flush();
        return buf;
      }
      break;
    case Key::CtrlC:
      buf.clear();
      cursor = 0;
      historyIndex_ = history_.size();
      std::cout << Color::Red << "^C" << Color::Reset << "\r\n";
      visibleLen = 0;
      redrawLine(prompt, buf, cursor, visibleLen);
      break;
    case Key::Backspace:
      if (cursor > 0) {
        buf.erase(cursor - 1, 1);
        --cursor;
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    case Key::AltBackspace:
      // Option-Backspace: delete the word behind the cursor, same as
      // readline's unix-word-rubout - skip trailing spaces first, then the
      // run of non-space characters before them.
      if (cursor > 0) {
        size_t start = cursor;
        while (start > 0 && buf[start - 1] == ' ') {
          --start;
        }
        while (start > 0 && buf[start - 1] != ' ') {
          --start;
        }
        buf.erase(start, cursor - start);
        cursor = start;
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    case Key::Left:
      if (cursor > 0) {
        --cursor;
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    case Key::Right:
      if (cursor < buf.size()) {
        ++cursor;
        redrawLine(prompt, buf, cursor, visibleLen);
      } else {
        // At end-of-line with nothing to move into: accept the inline
        // suggestion instead, the same way fish/zsh autosuggestions work.
        std::string suggestion = findUniqueSuggestion(buf);
        if (!suggestion.empty()) {
          buf = suggestion;
          cursor = buf.size();
          redrawLine(prompt, buf, cursor, visibleLen);
        }
      }
      break;
    case Key::Tab: {
      std::string suggestion = findUniqueSuggestion(buf);
      if (!suggestion.empty()) {
        buf = suggestion;
        cursor = buf.size();
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    }
    case Key::Up:
      // Step back one entry in history, stashing the in-progress line first
      // so Down can bring it back once we return to the bottom.
      if (historyIndex_ > 0) {
        if (historyIndex_ == history_.size()) {
          draft_ = buf;
        }
        --historyIndex_;
        buf = history_[historyIndex_];
        cursor = buf.size();
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    case Key::Down:
      if (historyIndex_ < history_.size()) {
        ++historyIndex_;
        buf = (historyIndex_ == history_.size()) ? draft_ : history_[historyIndex_];
        cursor = buf.size();
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    case Key::PasteStart: {
      // Insert the whole pasted block at once - embedded newlines were
      // already folded to spaces by readPastedText, so this can't trigger
      // Enter (early submit) or re-print the prompt line by line.
      std::string pasted = readPastedText();
      if (!pasted.empty()) {
        buf.insert(cursor, pasted);
        cursor += pasted.size();
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    }
    case Key::PasteEnd:
      // Stray end marker with no matching start - ignore.
      break;
    case Key::Char:
      if (ch == '@') {
        buf.insert(cursor, 1, '@');
        ++cursor;
        // Open the modal below the input line, accounting for the input
        // having wrapped onto more than one terminal row.
        int width = termWidth();
        size_t rows = width > 0 ? (prompt.size() + buf.size() - 1) / width : 0;
        for (size_t i = 0; i < rows + 1; ++i) {
          std::cout << "\r\n";
        }
        std::cout.flush();
        std::string picked = filePicker();
        // filePicker leaves the cursor on the line where its menu began.
        // Step back up so we redraw over the original prompt line(s) instead
        // of duplicating them below.
        std::cout << "\x1b[" << (rows + 1) << "A";
        if (!picked.empty()) {
          buf.insert(cursor, picked);
          cursor += picked.size();
        }
        visibleLen = 0;
        redrawLine(prompt, buf, cursor, visibleLen);
      } else {
        buf.insert(cursor, 1, ch);
        ++cursor;
        redrawLine(prompt, buf, cursor, visibleLen);
      }
      break;
    default:
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Command handling
// ---------------------------------------------------------------------------
bool Commander::handle(std::string input, Conversation &conversation,
                       Storage &storage, ProviderManager &provider) {
  if (input.empty()) {
    return true;
  }

  if (input == "history") {
    conversation.printHistory();
    return true;
  }
  if (input == "clear") {
    conversation.clearHistory();
    return true;
  }
  if (input == "save") {
    storage.save(conversation);
    return true;
  }

  if (input == "/provider") {
    static const std::vector<std::string> kProviders = {"openrouter", "ollama"};
    std::string current = provider.getCurrentProvider().getName();
    std::string picked = providerPicker(kProviders, current);
    if (picked.empty() || picked == current) {
      return true;
    }

    if (picked == "openrouter") {
      provider.registerProvider(std::make_shared<OpenRouter>());
    } else if (picked == "ollama") {
      provider.registerProvider(std::make_shared<Ollama>());
    }
    std::cout << Color::Green << "✓ " << Color::Reset << "Provider set: "
              << Color::Bold << picked << Color::Reset << std::endl;

    // Pick a starting model on the new gateway so the user isn't left with
    // an empty model until they run /model themselves.
    std::vector<std::string> models = provider.getCurrentProvider().getModels();
    if (!models.empty()) {
      provider.setModel(models.front());
      std::cout << Color::Green << "✓ " << Color::Reset << "Model set: "
                << Color::Bold << models.front() << Color::Reset << std::endl;
    }
    return true;
  }

  if (input == "/model" || input.substr(0, 7) == "/model ") {
    try {
      // Direct form: "/model <name>" sets the model without the picker.
      std::string arg = input.size() > 7 ? input.substr(7) : "";
      // trim surrounding whitespace
      auto notSpace = [](unsigned char c) { return !std::isspace(c); };
      arg.erase(arg.begin(), std::find_if(arg.begin(), arg.end(), notSpace));
      arg.erase(std::find_if(arg.rbegin(), arg.rend(), notSpace).base(), arg.end());
      if (!arg.empty()) {
        provider.setModel(arg);
        std::cout << Color::Green << "✓ " << Color::Reset << "Model set: "
                  << Color::Bold << arg << Color::Reset << std::endl;
        return true;
      }

      // Bare "/model": open the interactive picker.
      std::cout << Color::Dim << "Loading models..." << Color::Reset << std::flush;
      std::vector<std::string> models = provider.getCurrentProvider().getModels();
      std::cout << "\r\x1b[K"; // wipe the loading line
      if (models.empty()) {
        std::cout << Color::Yellow << "No models available." << Color::Reset << std::endl;
        return true;
      }
      std::string current = provider.getCurrentProvider().getModel();
      std::string picked = modelPicker(models, current);
      if (!picked.empty()) {
        provider.setModel(picked);
        std::cout << Color::Green << "✓ " << Color::Reset << "Model set: "
                  << Color::Bold << picked << Color::Reset << std::endl;
      }
      return true;
    } catch (const std::out_of_range& e) {
      return true;
    } catch (const std::runtime_error& e) {
      std::cout << Color::Red << "✗ " << Color::Reset << e.what() << std::endl;
      return true;
    }
  }



  return false;
}
