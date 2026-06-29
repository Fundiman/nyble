// Test 1: Basic throw and catch
let caught = false;
try {
    throw "error message";
} catch (e) {
    console.log("Caught:", e);
    caught = true;
}
console.log("Test 1 (basic catch):", caught ? "PASS" : "FAIL");

// Test 2: Throw number
try {
    throw 42;
} catch (e) {
    console.log("Caught number:", e);
}
console.log("Test 2 (throw number): PASS");

// Test 3: Nested try/catch
try {
    try {
        throw "inner";
    } catch (inner) {
        console.log("Inner caught:", inner);
        throw "rethrown";
    }
} catch (outer) {
    console.log("Outer caught:", outer);
}
console.log("Test 3 (nested): PASS");

// Test 4: Finally always runs
let finallyRan = false;
try {
    console.log("In try");
} finally {
    finallyRan = true;
    console.log("Finally ran");
}
console.log("Test 4 (finally no exception):", finallyRan ? "PASS" : "FAIL");

// Test 5: Finally runs after catch
let finallyAfterCatch = false;
try {
    throw "err";
} catch (e) {
    console.log("Catch:", e);
} finally {
    finallyAfterCatch = true;
    console.log("Finally after catch");
}
console.log("Test 5 (finally after catch):", finallyAfterCatch ? "PASS" : "FAIL");
