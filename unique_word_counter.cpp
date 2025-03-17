#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// Structure to pass arguments to threads
struct thread_args {
    char* file_content;
    size_t start;
    size_t end;
    std::set<std::string>* unique_words;
    std::mutex* mutex;
};

// Function to convert a word to lowercase and remove punctuation
std::string process_word(const std::string& word) {
    std::string processed_word;
    for (char c : word) {
        if (std::isalnum(c)) {
            processed_word += std::tolower(c);
        }
    }
    return processed_word;
}

// Thread function to count unique words in a portion of the file
void count_unique_words(thread_args* args) {
    std::set<std::string>* unique_words = args->unique_words;
    std::mutex* mutex = args->mutex;

    std::string word;
    std::istringstream iss(std::string(args->file_content + args->start, args->end - args->start));

    while (iss >> word) {
        word = process_word(word);
        if (!word.empty()) {
            std::lock_guard<std::mutex> lock(*mutex);
            unique_words->insert(word);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " filename num_threads" << std::endl;
        return 1;
    }

    int num_threads = std::stoi(argv[2]);

    // Open the file and map it to memory
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << argv[1] << std::endl;
        return 1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Failed to get file stats" << std::endl;
        close(fd);
        return 1;
    }

    char* file_content = (char*)mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (file_content == MAP_FAILED) {
        std::cerr << "Failed to map file to memory" << std::endl;
        close(fd);
        return 1;
    }

    // Calculate the portion of the file for each thread
    size_t file_size = sb.st_size;
    size_t portion_size = file_size / num_threads;

    // Create threads
    std::thread threads[num_threads];
    thread_args thread_args_array[num_threads];
    std::set<std::string> unique_words;
    std::mutex mutex;

    for (int i = 0; i < num_threads; i++) {
        size_t start = i * portion_size;
        size_t end = (i == num_threads - 1) ? file_size : (i + 1) * portion_size;

        thread_args_array[i].file_content = file_content;
        thread_args_array[i].start = start;
        thread_args_array[i].end = end;
        thread_args_array[i].unique_words = &unique_words;
        thread_args_array[i].mutex = &mutex;

        threads[i] = std::thread(count_unique_words, &thread_args_array[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    // Print the number of unique words
    std::cout << "Number of unique words: " << unique_words.size() << std::endl;

    // Clean up
    munmap(file_content, file_size);
    close(fd);

    return 0;
}
