#pragma once
#include <string>
#include <string_view>

// Small streaming Markdown-to-ANSI highlighter for terminal output. Not a
// full CommonMark renderer - just enough so a model's raw markdown (headings,
// bullets, **bold**, `code`, fenced ``` code blocks) reads as something other
// than a wall of literal asterisks and hashes.
//
// Built to be fed arbitrarily-sized chunks as they stream in from the model.
// Markers that straddle a chunk boundary (e.g. a "**" split across two
// network reads) are held back in `buf_` until enough of the next chunk
// arrives to resolve them, so nothing gets misrendered because of where a
// chunk happened to end.
class MarkdownStream {
public:
  std::string feed(std::string_view chunk) {
    buf_.append(chunk.data(), chunk.size());
    std::string out;
    size_t i = 0;
    while (i < buf_.size()) {
      char c = buf_[i];

      if (atLineStart_) {
        if (c == '`' && buf_.size() - i < 3) {
          break; // could still turn into a ``` fence - wait for more input
        }
        if (c == '`' && buf_[i + 1] == '`' && buf_[i + 2] == '`') {
          inFence_ = !inFence_;
          out += inFence_ ? "\x1b[2m```" : "```\x1b[0m";
          i += 3;
          atLineStart_ = false;
          continue;
        }
        if (!inFence_) {
          if (c == '#') {
            size_t j = i;
            while (j < buf_.size() && buf_[j] == '#' && (j - i) < 6) {
              ++j;
            }
            if (j == buf_.size()) {
              break; // could still gain more '#'
            }
            if (buf_[j] == ' ') {
              out += "\x1b[1;36m";
              heading_ = true;
              i = j + 1; // swallow "### "
              atLineStart_ = false;
              continue;
            }
            // Not a heading (e.g. "#5") - falls through to literal printing.
          } else if (c == '-' || c == '*') {
            if (i + 1 == buf_.size()) {
              break; // need to see whether a space follows
            }
            if (buf_[i + 1] == ' ') {
              out += "\x1b[36m•\x1b[0m ";
              i += 2;
              atLineStart_ = false;
              continue;
            }
            // Not a bullet (e.g. "*foo" or "-1") - falls through.
          }
        }
        atLineStart_ = false;
      }

      if (c == '\n') {
        if (heading_) {
          out += "\x1b[0m";
          heading_ = false;
        }
        out += '\n';
        atLineStart_ = true;
        ++i;
        continue;
      }

      if (inFence_) {
        // No inline styling inside fenced code - a stray '#'/'*'/'`' in real
        // code is not markdown, and would otherwise get misread as one.
        out += c;
        ++i;
        continue;
      }

      if (c == '`') {
        code_ = !code_;
        out += code_ ? "\x1b[33m" : "\x1b[39m"; // fg-only, doesn't clobber bold/heading
        ++i;
        continue;
      }
      if (c == '*') {
        if (i + 1 == buf_.size()) {
          break; // could still turn into "**"
        }
        if (buf_[i + 1] == '*') {
          bold_ = !bold_;
          out += bold_ ? "\x1b[1m" : "\x1b[22m";
          i += 2;
          continue;
        }
        out += c; // lone '*' - no italics support, print literally
        ++i;
        continue;
      }
      out += c;
      ++i;
    }
    buf_.erase(0, i);
    return out;
  }

  // Call once a response is fully done streaming: flushes anything still
  // held back (e.g. a trailing '#' that never got its space) as plain text
  // and closes out any style left open by a truncated stream, so the next
  // thing printed doesn't inherit stale color/bold.
  std::string finish() {
    std::string out = buf_;
    buf_.clear();
    if (heading_ || bold_ || code_ || inFence_) {
      out += "\x1b[0m";
    }
    heading_ = bold_ = code_ = inFence_ = false;
    atLineStart_ = true;
    return out;
  }

private:
  std::string buf_;
  bool atLineStart_ = true;
  bool bold_ = false;
  bool code_ = false;
  bool heading_ = false;
  bool inFence_ = false;
};
