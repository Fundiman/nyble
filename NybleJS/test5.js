// test5.js - Advanced NybleJS Engine Test Suite

console.log("=== NybleJS Hybrid Engine Test Suite ===");
console.log("");

// 1. Let/Const Scoping Tests
console.log("--- Block Scoping Tests ---");
{
    let blockScoped = "I'm in a block!";
    const CONSTANT_VALUE = 42;
    console.log(blockScoped);
    console.log("Constant:", CONSTANT_VALUE);
}
// console.log(blockScoped); // Uncomment would cause ReferenceError

// 2. Function Scoping with let/const
function testScoping() {
    let functionScoped = "Function scope";
    const PI = 3.14159;
    if (true) {
        let innerScoped = "Inner block";
        const INNER_CONST = 100;
        console.log(innerScoped);
        console.log(INNER_CONST);
    }
    console.log(functionScoped);
    console.log(PI);
}
testScoping();

// 3. Arrow Functions
console.log("\n--- Arrow Functions ---");
const multiply = (a, b) => a * b;
console.log("5 * 3 =", multiply(5, 3));

const square = x => x * x;
console.log("12² =", square(12));

// 4. Array Methods with Arrow Functions
console.log("\n--- Array Operations ---");
const numbers = [10, 20, 30, 40, 50];
const doubled = numbers.map(n => n * 2);
const filtered = numbers.filter(n => n > 25);
const sum = numbers.reduce((acc, curr) => acc + curr, 0);

console.log("Original:", numbers);
console.log("Doubled:", doubled);
console.log("Filtered (>25):", filtered);
console.log("Sum:", sum);

// 5. Object Literals and Destructuring
console.log("\n--- Object Operations ---");
const person = {
    name: "Alice",
    age: 30,
    hobbies: ["reading", "coding", "gaming"],
    address: {
        city: "Wonderland",
        zip: 12345
    }
};

const { name, age, hobbies } = person;
console.log(`Name: ${name}, Age: ${age}`);
console.log("Hobbies:", hobbies.join(", "));

// 6. Template Literals
console.log("\n--- Template Literals ---");
const greeting = `Hello ${name}! You are ${age} years old.`;
console.log(greeting);

// 7. Spread Operator
console.log("\n--- Spread Operator ---");
const arr1 = [1, 2, 3];
const arr2 = [4, 5, 6];
const combined = [...arr1, ...arr2];
console.log("Combined arrays:", combined);

// 8. Default Parameters
console.log("\n--- Default Parameters ---");
function greet(name = "Guest", greeting = "Hello") {
    return `${greeting}, ${name}!`;
}
console.log(greet("Bob"));
console.log(greet(undefined, "Hi"));

// 9. Performance Test (while loop with let)
console.log("\n--- Performance Test ---");
const startTime = Date.now();
let counter = 0;
while (counter < 1000000) {
    counter++;
}
const endTime = Date.now();
console.log(`Counted to 1,000,000 in ${endTime - startTime}ms`);

// 10. Nested Functions with Closures
console.log("\n--- Closures ---");
function createCounter() {
    let count = 0;
    return {
        increment: () => ++count,
        decrement: () => --count,
        getCount: () => count
    };
}

const counterObj = createCounter();
console.log("Counter:", counterObj.getCount());
counterObj.increment();
counterObj.increment();
console.log("After 2 increments:", counterObj.getCount());
counterObj.decrement();
console.log("After 1 decrement:", counterObj.getCount());

// 11. Try-Catch-Finally
console.log("\n--- Error Handling ---");
try {
    console.log("Trying something risky...");
    // throw new Error("Something went wrong!");
    console.log("Success!");
} catch (e) {
    console.log("Caught error:", e.message);
} finally {
    console.log("This always runs!");
}

// 12. Type Coercion
console.log("\n--- Type Coercion ---");
console.log("5" + 3); // "53"
console.log("5" - 3); // 2
console.log("5" * "3"); // 15
console.log(5 + Number("3")); // 8

// 13. FizzBuzz (Classic)
console.log("\n--- FizzBuzz (First 20) ---");
for (let i = 1; i <= 20; i++) {
    let output = "";
    if (i % 3 === 0) output += "Fizz";
    if (i % 5 === 0) output += "Buzz";
    console.log(i + ":", output || i);
}

// 14. Recursion
console.log("\n--- Recursion ---");
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
console.log("5! =", factorial(5));
console.log("10! =", factorial(10));

// 15. String Methods
console.log("\n--- String Methods ---");
const text = "   Hello NybleJS World!   ";
console.log("Trimmed:", text.trim());
console.log("Uppercase:", text.toUpperCase());
console.log("Lowercase:", text.toLowerCase());
console.log("Contains 'Nyble':", text.includes("Nyble"));
console.log("Split:", text.trim().split(" "));

// 16. Math Operations
console.log("\n--- Math Operations ---");
console.log("PI:", Math.PI);
console.log("Random:", Math.random());
console.log("Max:", Math.max(10, 20, 30, 40));
console.log("Min:", Math.min(10, 20, 30, 40));
console.log("Floor:", Math.floor(3.7));
console.log("Ceil:", Math.ceil(3.2));
console.log("Round:", Math.round(3.5));

// 17. Date Operations
console.log("\n--- Date Operations ---");
const now = new Date();
console.log("Current Date:", now.toString());
console.log("Year:", now.getFullYear());
console.log("Month:", now.getMonth() + 1);
console.log("Day:", now.getDate());

// 18. Array Destructuring
console.log("\n--- Array Destructuring ---");
const [first, second, ...rest] = [1, 2, 3, 4, 5];
console.log("First:", first);
console.log("Second:", second);
console.log("Rest:", rest);

// 19. Object Property Shorthand
console.log("\n--- Object Property Shorthand ---");
const x = 10;
const y = 20;
const point = { x, y };
console.log("Point:", point);

// 20. IIFE (Immediately Invoked Function Expression)
console.log("\n--- IIFE ---");
const result = (function() {
    let secret = "hidden data";
    return {
        getSecret: () => secret
    };
})();
console.log("IIFE Result:", result.getSecret());

console.log("\n=== All Tests Completed Successfully ===");
console.log("NybleJS is working perfectly!");