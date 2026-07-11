#include "commander.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Raw terminal helpers
// ---------------------------------------------------------------------------
namespace {

// RAII guard: put the terminal in raw mode (no echo, no line buffering) for the
// lifetime of the object and restore the previous settings on destruction, even
// if an exception unwinds through us.
class RawMode {
public:
  RawMode() : ok_(false) {
    if (tcgetattr(STDIN_FILENO, &orig_) == -1) {
      return;
    }
    termios raw = orig_;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
      return;
    }
    ok_ = true;
  }
  ~RawMode() {
    if (ok_) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_);
    }
  }
  bool ok() const { return ok_; }

private:
  termios orig_{};
  bool ok_;
};

enum class Key {
  Char,
  Enter,
  Backspace,
  Up,
  Down,
  Left,
  Right,
  Escape,
  Tab,
  CtrlC,
  CtrlD,
  Unknown,
};

// Read one logical keypress, decoding ANSI escape sequences for arrow keys.
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
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
      return Key::Escape;
    }
    if (seq[0] != '[' && seq[0] != 'O') {
      out = seq[0];
      return Key::Escape;
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
      return Key::Escape;
    }
    switch (seq[1]) {
    case 'A':
      return Key::Up;
    case 'B':
      return Key::Down;
    case 'C':
      return Key::Right;
    case 'D':
      return Key::Left;
    default:
      return Key::Unknown;
    }
  }
  default:
    out = c;
    return Key::Char;
  }
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

    std::string header = "\x1b[2m  @" + base + filter +
                         "  (↑↓ move · → open · ⏎/space pick · esc cancel)\x1b[0m\r\n";
    std::cout << header;
    int lines = 1;

    int first = 0;
    if (selected >= kWindow) {
      first = selected - kWindow + 1;
    }
    int last = std::min(static_cast<int>(entries.size()), first + kWindow);
    for (int i = first; i < last; ++i) {
      const Entry& e = entries[i];
      std::string label = "  "+ e.name + (e.is_dir ? "/" : "");
      if (i == selected) {
        std::cout << "\x1b[7m" << label << "\x1b[0m\r\n"; // reverse video
      } else {
        std::cout << label << "\r\n";
      }
      ++lines;
    }
    if (entries.empty()) {
      std::cout << "\x1b[2m  (no matches)\x1b[0m\r\n";
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

    std::cout << "\x1b[2m  model: " << (filter.empty() ? "" : filter)
              << "  (↑↓ move · ⏎ pick · esc cancel)\x1b[0m\r\n";
    int lines = 1;

    int first = 0;
    if (selected >= kWindow) {
      first = selected - kWindow + 1;
    }
    int last = std::min(static_cast<int>(view.size()), first + kWindow);
    for (int i = first; i < last; ++i) {
      const std::string& m = *view[i];
      bool isCurrent = (m == current);
      std::string label = std::string("  ") + (isCurrent ? "* " : "  ") + m;
      if (i == selected) {
        std::cout << "\x1b[7m" << label << "\x1b[0m\r\n"; // reverse video
      } else if (isCurrent) {
        std::cout << "\x1b[1m" << label << "\x1b[0m\r\n"; // bold current
      } else {
        std::cout << label << "\r\n";
      }
      ++lines;
    }
    if (view.empty()) {
      std::cout << "\x1b[2m  (no matches)\x1b[0m\r\n";
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

// Redraw the input line: carriage return, clear line, prompt + buffer, then
// place the terminal cursor at `cursor` within the buffer.
void redrawLine(const std::string& prompt, const std::string& buf, size_t cursor) {
  std::cout << "\r\x1b[K" << prompt << buf;
  size_t tail = buf.size() - cursor;
  if (tail > 0) {
    std::cout << "\x1b[" << tail << "D";
  }
  std::cout.flush();
}

} // namespace

// ---------------------------------------------------------------------------
// Public interactive reader
// ---------------------------------------------------------------------------
std::string Commander::readLine(const std::string& prompt, bool& eof) {
  eof = false;
  RawMode raw;
  if (!raw.ok()) {
    // Not a tty (e.g. piped input) -> fall back to plain line reading.
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {
      eof = true;
    }
    return line;
  }

  std::string buf;
  size_t cursor = 0;
  redrawLine(prompt, buf, cursor);

  while (true) {
    char ch = 0;
    Key k = readKey(ch);
    switch (k) {
    case Key::Enter:
      std::cout << "\r\n";
      std::cout.flush();
      return buf;
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
      std::cout << "^C\r\n";
      redrawLine(prompt, buf, cursor);
      break;
    case Key::Backspace:
      if (cursor > 0) {
        buf.erase(cursor - 1, 1);
        --cursor;
        redrawLine(prompt, buf, cursor);
      }
      break;
    case Key::Left:
      if (cursor > 0) {
        --cursor;
        redrawLine(prompt, buf, cursor);
      }
      break;
    case Key::Right:
      if (cursor < buf.size()) {
        ++cursor;
        redrawLine(prompt, buf, cursor);
      }
      break;
    case Key::Char:
      if (ch == '@') {
        buf.insert(cursor, 1, '@');
        ++cursor;
        std::cout << "\r\n"; // open modal one line below the input line
        std::cout.flush();
        std::string picked = filePicker();
        // filePicker leaves the cursor on the line where its menu began (one
        // below the prompt). Step back up so we redraw over the original prompt
        // line instead of duplicating it below.
        std::cout << "\x1b[1A";
        if (!picked.empty()) {
          buf.insert(cursor, picked);
          cursor += picked.size();
        }
        redrawLine(prompt, buf, cursor);
      } else {
        buf.insert(cursor, 1, ch);
        ++cursor;
        redrawLine(prompt, buf, cursor);
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
        std::cout << "Model set: " << arg << std::endl;
        return true;
      }

      // Bare "/model": open the interactive picker.
      std::cout << "Loading models..." << std::flush;
      std::vector<std::string> models = provider.getCurrentProvider().getModels();
      std::cout << "\r\x1b[K"; // wipe the loading line
      if (models.empty()) {
        std::cout << "No models available." << std::endl;
        return true;
      }
      std::string current = provider.getCurrentProvider().getModel();
      std::string picked = modelPicker(models, current);
      if (!picked.empty()) {
        provider.setModel(picked);
        std::cout << "Model set: " << picked << std::endl;
      }
      return true;
    } catch (const std::out_of_range& e) {
      return true;
    } catch (const std::runtime_error& e) {
      std::cout << e.what() << std::endl;
      return true;
    }
  }



  return false;
}
