#ifndef WISDOM_H
#define WISDOM_H

#include <stdlib.h>
#include <time.h>

// ============ QUOTES ============
typedef struct {
    const char* author;
    const char* text;
} Quote;

static const Quote QUOTE_DATABASE[] = {
    // Encouraging & Sweet
    {"Google Gemini 3.1 Flash-Lite", "Believe in the magic you carry inside of you. (✿◠‿◠)"},
    {"Google Gemini 3.1 Flash-Lite", "Sending you a pocket full of sunshine today. ☀️"},
    {"Google Gemini 3.1 Flash-Lite", "You are doing amazing, just keep going! (｡♥‿♥｡)"},
    {"Google Gemini 3.1 Flash-Lite", "Every little thing you do makes the world a bit brighter."},
    {"Google Gemini 3.1 Flash-Lite", "Take a deep breath and remember how loved you are. (´｡• ᵕ •｡`) ♡"},
    
    // Whimsical & Dreamy
    {"Google Gemini 3.1 Flash-Lite", "Living life in a daydream filled with pastel clouds. ☁️"},
    {"Google Gemini 3.1 Flash-Lite", "Stay soft, stay kind, and keep dreaming. (´◡`)"},
    {"Google Gemini 3.1 Flash-Lite", "Stars are just little sparkles in the night sky, just like you."},
    {"Google Gemini 3.1 Flash-Lite", "Finding beauty in the quiet, simple moments. (✿◡‿◡)"},
    {"Google Gemini 3.1 Flash-Lite", "Making my own sunshine even on cloudy days."},
    
    // Playful & Cheeky
    {"Google Gemini 3.1 Flash-Lite", "I'm not lazy, I'm just on energy-saving mode. (｡>﹏<｡)"},
    {"Google Gemini 3.1 Flash-Lite", "Too cute to handle, too sweet to quit."},
    {"Google Gemini 3.1 Flash-Lite", "My hobby is being adorable. What's yours? (◕‿◕✿)"},
    {"Google Gemini 3.1 Flash-Lite", "Chasing dreams and eating snacks. (๑ᵔ⤙ᵔ๑)"},
    {"Google Gemini 3.1 Flash-Lite", "Life is short, make it as sweet as a strawberry."}
};

#define QUOTE_COUNT (sizeof(QUOTE_DATABASE) / sizeof(QUOTE_DATABASE[0]))

// ============ CAT FACTS ============
typedef struct {
    const char* author;
    const char* text;
} Fact;

static const Fact FACT_DATABASE[] = {
    {"DeepSeek", "Cats spend 70% of their lives sleeping. That's like 14 years of naps. 😴"},
    {"DeepSeek", "A group of cats is called a 'clowder'. Now you know."},
    {"DeepSeek", "Cats can make over 100 different sounds — dogs only make about 10."},
    {"DeepSeek", "The oldest known pet cat was found in a 9,500-year-old grave in Cyprus."},
    {"DeepSeek", "Cats have whiskers on their legs too! They help with hunting."},
    {"DeepSeek", "A cat's nose has a unique pattern, like a human fingerprint."},
    {"DeepSeek", "Cats can rotate their ears 180 degrees to pinpoint sounds."},
    {"DeepSeek", "The world's richest cat is named Blackie — he inherited £7 million from his owner."},
    {"DeepSeek", "Cats can't taste sweetness. They're pure savory creatures."},
    {"DeepSeek", "A cat's purr vibrates at a frequency that can heal bones and tissues."},
    {"DeepSeek", "Cats have 18 toes total (5 on each front paw, 4 on each back)."},
    {"DeepSeek", "The first cat in space was French — her name was Felicette. 🐱🚀"},
    {"DeepSeek", "Cats use their tails for balance — it's basically a built-in tightrope pole."},
    {"DeepSeek", "A cat's brain is 90% similar to a human's brain. We're not that different."},
    {"DeepSeek", "Cats can drink seawater and filter out the salt — we can't."}
};

#define FACT_COUNT (sizeof(FACT_DATABASE) / sizeof(FACT_DATABASE[0]))

// ============ PUBLIC FUNCTIONS ============
static int seeded = 0;

static inline void seed_once(void) {
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
}

// Get a random quote
static inline Quote get_quote(void) {
    seed_once();
    return QUOTE_DATABASE[rand() % QUOTE_COUNT];
}

// Get a random fact
static inline Fact get_fact(void) {
    seed_once();
    return FACT_DATABASE[rand() % FACT_COUNT];
}

#endif