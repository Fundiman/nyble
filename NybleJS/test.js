console.log("Hello from NybleJS!");

// Variables
let x = 42;
const y = "hello";
var z = true;
console.log(x);
console.log(y);
console.log(z);

// Arithmetic
console.log(1 + 2 * 3);
console.log(10 / 3);
console.log(2 ** 8);

// Functions
function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
console.log("fib(10) =", fib(10));

// Closures
function makeCounter() {
    let count = 0;
    return function() {
        count = count + 1;
        return count;
    };
}
let c = makeCounter();
console.log(c());
console.log(c());
console.log(c());

// Arrays
let arr = [1, 2, 3, 4, 5];
console.log(arr.length);
console.log(arr.map(x => x * 2));
console.log(arr.filter(x => x > 2));

// Objects
let obj = {a: 1, b: 2, c: 3};
console.log(Object.keys(obj));
console.log(obj.a);

// Strings
let s = "hello world";
console.log(s.length);
console.log(s.toUpperCase());
console.log(s.indexOf("world"));

// Math
console.log(Math.PI);
console.log(Math.sqrt(144));
console.log(Math.floor(3.7));

// Loops
let sum = 0;
for (let i = 0; i < 10; i++) {
    sum = sum + i;
}
console.log("sum 0-9 =", sum);

// While
let i = 0;
while (i < 5) {
    console.log("while:", i);
    i = i + 1;
}

console.log("All tests passed!");
