// GC stress test: VM path with heavy allocation
function allocLoop(n) {
    var arr = [];
    var i = 0;
    while (i < n) {
        arr.push({a: 1, b: "hello", c: [1, 2, 3]});
        i = i + 1;
    }
    return arr.length;
}

// Allocate and GC repeatedly
var total = 0;
var j = 0;
while (j < 20) {
    total = total + allocLoop(500 + j * 100);
    j = j + 1;
}
console("stress alloc result: " + total);

// String churn
var s = "x";
var k = 0;
while (k < 15) {
    s = s + s;
    k = k + 1;
}
console("string length: " + s.length);
console("stress test passed");
