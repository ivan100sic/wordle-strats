#include <string>
#include <fstream>
#include <iostream>
#include <array>
#include <span>
#include <algorithm>
#include <bitset>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <numeric>
#include <set>
#include <ranges>

class ThreadPool {
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;
    std::mutex mtx_;

    void run() {
        mtx_.lock();
        while (queue_.size()) {
            auto func = std::move(queue_.front());
            queue_.pop();
            mtx_.unlock();
            func();
            mtx_.lock();
        }
        mtx_.unlock();
    }

public:

    /* 
    * Create a new thread pool, an object which can execute tasks concurrently
    * @param threads Number of threads, defaults to the number of threads available on the machine.
    */ 
    ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
        threads = std::max(threads, (size_t)1);
        threads_.resize(threads);
        for (auto& thread : threads_) {
            thread = std::thread([this]() { run(); });
        }
    }

    /* 
    * Submit a function to be executed.
    */    
    void submit(std::function<void()> func) {
        std::unique_lock lock(mtx_);
        queue_.push(std::move(func));
    }
    
    /* 
    * Wait until all work has been completed.
    */
    void wait() {
        for (auto& thread : threads_) {
            thread.join();
        }
    }
};

/* 
 * Reads the contents of the file. The function is binary-safe.
 *
 * @param file_name Path to the file
 * @return String containing the data.
 */
std::string read_whole_file(const std::string& file_name) {
    std::ifstream file(file_name);
    return std::string(std::istreambuf_iterator<char>{file}, {});
}

class Marks {
    // Bitmask of marks - five groups of two bits.
    // 0 means undefined.
    // 1 means the letter doesn't exist in the solution.
    // 2 means the letter exists but it's in the wrong spot.
    // 3 means the letter is in the right spot.
    uint32_t marks_;

    void set(int i, uint32_t mask) {
        marks_ &= ~((uint32_t)3 << (2*i));
        marks_ ^= mask << (2*i);
    }
public:
    Marks() : marks_(0) {}

    void set_correct(int i) {
        set(i, 3);
    }

    void set_wrong_spot(int i) {
        set(i, 2);
    }

    void set_none(int i) {
        set(i, 1);
    }

    bool operator== (const Marks& other) const = default;
    std::strong_ordering operator<=> (const Marks& other) const = default;
};

class Word {
    std::array<char, 5> word_;
public:
    Word(std::span<const char, 5> data) {
        std::ranges::copy(data, word_.begin());
    }

    Marks compute_marks(const Word& target) {
        Marks marks;
        std::bitset<5> used_target{}, used_word{};
        for (int i = 0; i < 5; i++) {
            if (word_[i] == target.word_[i]) {
                marks.set_correct(i);
                used_target.set(i);
                used_word.set(i);
            }
        }

        for (int i = 0; i < 5; i++) {
            if (!used_word[i]) {
                for (int j = 0; j < 5; j++) {
                    if (word_[i] == target.word_[j] && !used_target[j]) {
                        used_word.set(i);
                        used_target.set(j);
                    }
                }
            }
        }

        for (int i = 0; i < 5; i++) {
            if (!used_word[i]) {
                marks.set_none(i);
            }
        }

        return marks;
    }

    std::string_view to_string_view() const {
        return {word_.begin(), word_.end()};
    }

    bool operator== (const Word& other) const = default;
    std::strong_ordering operator<=> (const Word& other) const = default;
};

std::vector<Word> parse_words(std::span<const char> file_contents) {
    const size_t none = ~(size_t)0;
    size_t last_quote = none;
    std::vector<Word> words;
    for (size_t i = 0; i < file_contents.size(); i++) {
        if (file_contents[i] == '"') {
            if (last_quote == none) {
                last_quote = i;
            } else if (i - last_quote == 6) {
                words.emplace_back(file_contents.subspan(last_quote + 1).subspan<0, 5>());
                last_quote = none;
            } else {
                // Bad word or parsing error
            }
        }
    }
    return words;
}

std::vector<int64_t> compute_sum_of_squares_ranks(std::span<Word> words, std::span<Word> targets) {
    std::vector<int64_t> result(words.size());
    ThreadPool pool;
    for (size_t i = 0; i < words.size(); i++) {
        pool.submit([i, words, targets, &result]() {
            std::map<Marks, int64_t> marks_count;
            for (const auto& target : targets) {
                marks_count[words[i].compute_marks(target)]++;
            }
            int64_t sum_of_squares = 0;
            for (const auto& [marks, count] : marks_count) {
                sum_of_squares += count * count;
            }
            result[i] = sum_of_squares;
        });
    }
    pool.wait();
    return result;
}

void print_best_sum_of_squares_ranks(std::span<Word> words, std::span<int64_t> ranks, size_t show_count = 10) {
    const size_t n = words.size();
    show_count = std::min(show_count, n);
    std::vector<size_t> indices_store(n);
    std::span indices(indices_store);
    std::iota(indices.begin(), indices.end(), (size_t)0);

    auto compare = [ranks](size_t i, size_t j) {
        return ranks[i] < ranks[j];
    };

    std::ranges::nth_element(indices, indices.begin() + show_count, compare);
    auto best_indices = indices.subspan(0, show_count);
    std::ranges::sort(best_indices, compare);
    
    for (size_t idx : best_indices) {
        std::cout << words[idx].to_string_view() << ' ' << ranks[idx] << '\n';
    }
}

int main() {
    auto words = parse_words(read_whole_file("words.txt"));
    auto targets = parse_words(read_whole_file("targets.txt"));
    auto ranks = compute_sum_of_squares_ranks(words, targets);
    print_best_sum_of_squares_ranks(words, ranks, -1);
}