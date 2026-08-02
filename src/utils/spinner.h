#pragma once
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Braille spinner + rotating whimsical status word, shown while waiting on
// the network for the model's first byte (request latency, not generation
// itself - once tokens start streaming there's nothing left to wait on).
// Runs on its own thread since the caller is blocked inside a synchronous
// cpr::Post for that whole window.
class Spinner {
public:
  Spinner()
      : words_{"Thinking",  "Pondering",     "Noodling",   "Ruminating",
               "Cogitating", "Percolating",   "Marinating", "Scheming",
               "Conjuring",  "Contemplating", "Simmering",  "Whirring"} {}

  void start() {
    if (running_) {
      return;
    }
    running_ = true;
    thread_ = std::thread([this] { run(); });
  }

  // Idempotent: safe to call even if start() was never called, or if this
  // is a repeat call after an earlier stop().
  void stop() {
    if (!running_) {
      return;
    }
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
    std::cout << "\r\x1b[K" << std::flush; // erase the spinner line
  }

  ~Spinner() { stop(); }

private:
  void run() {
    static const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                   "⠴", "⠦", "⠧", "⠇", "⠏"};
    size_t frame = 0;
    size_t wordIdx = 0;
    auto lastWordChange = std::chrono::steady_clock::now();
    while (running_) {
      std::cout << "\r\x1b[K\x1b[2m" << frames[frame % 10] << " "
                << words_[wordIdx % words_.size()] << "...\x1b[0m"
                << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      ++frame;
      auto now = std::chrono::steady_clock::now();
      if (now - lastWordChange > std::chrono::milliseconds(1600)) {
        ++wordIdx;
        lastWordChange = now;
      }
    }
  }

  std::vector<std::string> words_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};
