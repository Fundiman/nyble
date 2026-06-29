// Test 6: Return inside try with finally
function testReturnInTry() {
    try {
        return "from try";
    } finally {
        console.log("finally in return test");
    }
}
let r = testReturnInTry();
console.log("Test 6 (return in try):", r === "from try" ? "PASS" : "FAIL");

// Test 7: Throw in catch with finally
try {
    try {
        throw "first";
    } catch (e) {
        console.log("Caught:", e);
        throw "second";
    } finally {
        console.log("Finally after catch throw");
    }
} catch (e2) {
    console.log("Outer caught:", e2);
}
console.log("Test 7 (throw in catch + finally): PASS");

// Test 8: Continue through finally
for (let i = 0; i < 3; i++) {
    try {
        if (i === 1) continue;
        console.log("i =", i);
    } finally {
        console.log("finally for i =", i);
    }
}
console.log("Test 8 (continue through finally): PASS");

// Test 9: Break through finally
for (let i = 0; i < 5; i++) {
    try {
        if (i === 2) break;
        console.log("break test i =", i);
    } finally {
        console.log("break finally for i =", i);
    }
}
console.log("Test 9 (break through finally): PASS");

// Test 10: Uncaught exception
let threw = false;
try {
    throw "uncaught test";
} catch (e) {
    console.log("Caught in test 10:", e);
    threw = true;
}
console.log("Test 10 (uncaught basic):", threw ? "PASS" : "FAIL");
