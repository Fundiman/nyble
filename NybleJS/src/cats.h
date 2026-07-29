#ifndef WISDOM_H
#define WISDOM_H

#include <stdlib.h>
#include <time.h>

typedef struct {
    const char* author;
    const char* text;
} Entry;

static const Entry WISDOM_DATABASE[] = {
    // Encouraging & Sweet
    {"Google Gemini 3.1 Flash-Lite", "Believe in the magic you carry inside of you. (✿‿◠)"},
    {"Google Gemini 3.1 Flash-Lite", "Sending you a pocket full of sunshine today."},
    {"Google Gemini 3.1 Flash-Lite", "You are doing amazing, just keep going! (｡♥‿♥｡)"},
    {"Google Gemini 3.1 Flash-Lite", "Every little thing you do makes the world a bit brighter."},
    {"Google Gemini 3.1 Flash-Lite", "Take a deep breath and remember how loved you are. (´｡• ᵕ •｡`) ♡"},

    // Whimsical & Dreamy
    {"Google Gemini 3.1 Flash-Lite", "Living life in a daydream filled with pastel clouds."},
    {"Google Gemini 3.1 Flash-Lite", "Stay soft, stay kind, and keep dreaming. (´◡`)"},
    {"Google Gemini 3.1 Flash-Lite", "Stars are just little sparkles in the night sky, just like you."},
    {"Google Gemini 3.1 Flash-Lite", "Finding beauty in the quiet, simple moments. (✿◡‿◡)"},
    {"Google Gemini 3.1 Flash-Lite", "Making my own sunshine even on cloudy days."},

    // Playful & Cheeky
    {"Google Gemini 3.1 Flash-Lite", "I'm not lazy, I'm just on energy-saving mode. (｡>﹏<｡)"},
    {"Google Gemini 3.1 Flash-Lite", "Too cute to handle, too sweet to quit."},
    {"Google Gemini 3.1 Flash-Lite", "My hobby is being adorable. What's yours? (◕‿◕✿)"},
    {"Google Gemini 3.1 Flash-Lite", "Chasing dreams and eating snacks. (๑ᵔ⤙ᵔ๑)"},
    {"Google Gemini 3.1 Flash-Lite", "Life is short, make it as sweet as a strawberry."},

    // Cat Facts
    {"DeepSeek", "Cats spend 70% of their lives sleeping. That's like 14 years of naps."},
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
    {"DeepSeek", "The first cat in space was French — her name was Felicette."},
    {"DeepSeek", "Cats use their tails for balance — it's basically a built-in tightrope pole."},
    {"DeepSeek", "A cat's brain is 90% similar to a human's brain. We're not that different."},
    {"DeepSeek", "Cats can drink seawater and filter out the salt — we can't."},

    // Cat Jokes
    {"opencode/big-pickle", "What do you call a cat that loves bowling? An alley cat."},
    {"opencode/big-pickle", "What do cats eat for breakfast? Mice Krispies."},
    {"opencode/big-pickle", "Why are cats bad at poker? They're always showing their paws."},
    {"opencode/big-pickle", "What do you call a pile of kittens? A meowtain."},
    {"opencode/big-pickle", "Why did the cat sit on the computer? To keep an eye on the mouse."},
    {"opencode/big-pickle", "Why do cats make terrible storytellers? They only have one tail."},
    {"opencode/big-pickle", "Why don't cats play poker in the jungle? Too many cheetahs."},
    {"opencode/big-pickle", "Why did the cat run away from the tree? It was afraid of the bark."},
    {"opencode/big-pickle", "What do cats use to drink coffee? A saucer."},
    {"opencode/big-pickle", "What's a cat's favorite part of a computer? The mouse pad."},
    {"opencode/big-pickle", "What do you call a cat with no legs? It doesn't matter, it won't come when you call."},
    {"opencode/big-pickle", "Why did the cat join Instagram? To stalk people pawfficially."},
    {"opencode/big-pickle", "Why was the cat so small? It was on a kitten diet."},
    {"opencode/big-pickle", "What do you call a cat who tells jokes? A stand-up meow-median."},
    {"opencode/big-pickle", "Why did the cat refuse to play cards? It was afraid of the wild cat-d."},
    {"opencode/big-pickle", "What's a cat's favorite subject in school? Hisss-tory."},
    {"opencode/big-pickle", "Why did the cat bring a ladder? To reach the high meows-ic notes."},
    {"opencode/big-pickle", "Why did the cat go to therapy? It had too many cat-astrophic thoughts."},
    {"opencode/big-pickle", "What do you call a cat who can't stop singing? A purr-atic performer."},
    {"opencode/big-pickle", "Why did the cat get kicked out of the fish store? It kept trying to sample the merchandise."},
    {"opencode/big-pickle", "Why do cats always win at hide and seek? Because they're purr-fect at blending into the couch."},
    {"opencode/big-pickle", "What did the cat say to the computer? 'Is that mouse edible?'"},
    {"opencode/big-pickle", "Why did the cat sit on the printer? It wanted to keep an eye on the mouse and paper."},
    {"opencode/big-pickle", "Why do cats make terrible roommates? They never clean up their fur-balls."},
    {"opencode/big-pickle", "What do you call a cat that can pick locks? A purr-cracker."}
};

#define WISDOM_COUNT (sizeof(WISDOM_DATABASE) / sizeof(WISDOM_DATABASE[0]))

static int seeded = 0;

static inline void seed_once(void) {
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
}

static inline Entry get_wisdom(void) {
    seed_once();
    return WISDOM_DATABASE[rand() % WISDOM_COUNT];
}

#endif
