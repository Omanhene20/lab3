#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <crypt.h>
#include <stdbool.h>

#define MAX_KEYSIZE 8
#define ALPHABET "abcdefghijklmnopqrstuvwxyz"
#define ALPHABET_LENGTH 26

const char* target_hash;
char salt[3];
int keysize;
bool found = false;
pthread_mutex_t lock;

// Thread argument structure
typedef struct {
    int thread_id;
    int total_threads;
} thread_args_t;

// Increment string like a base-26 number
int increment(char* str, int len) {
    for (int i = len - 1; i >= 0; i--) {
        if (str[i] != 'z') {
            str[i]++;
            return 1;
        }
        str[i] = 'a';
    }
    return 0; // overflow
}

// Thread function
void* crack_thread(void* arg) {
    thread_args_t* args = (thread_args_t*) arg;
    struct crypt_data data;
    data.initialized = 0;

    for (int len = 1; len <= keysize && !found; len++) {
        char* guess = malloc(len + 1);
        for (int i = 0; i < len; i++) {
            guess[i] = 'a';
        }
        guess[len] = '\0';

        // Assign starting character range based on thread ID
        for (int c = args->thread_id; c < ALPHABET_LENGTH; c += args->total_threads) {
            guess[0] = ALPHABET[c];
            do {
                pthread_mutex_lock(&lock);
                if (found) {
                    pthread_mutex_unlock(&lock);
                    free(guess);
                    return NULL;
                }
                pthread_mutex_unlock(&lock);

                char* result = crypt_r(guess, salt, &data);
                if (strcmp(result, target_hash) == 0) {
                    pthread_mutex_lock(&lock);
                    if (!found) {
                        found = true;
                        printf("Cracked: %s\n", guess);
                    }
                    pthread_mutex_unlock(&lock);
                    free(guess);
                    return NULL;
                }
            } while (increment(guess + 1, len - 1)); // increment rest of string
        }

        free(guess);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <threads> <keysize> <target>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    keysize = atoi(argv[2]);
    target_hash = argv[3];
    strncpy(salt, target_hash, 2);
    salt[2] = '\0';

    pthread_mutex_init(&lock, NULL);

    pthread_t threads[num_threads];
    thread_args_t args[num_threads];

    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].total_threads = num_threads;
        pthread_create(&threads[i], NULL, crack_thread, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);

    return 0;
}
