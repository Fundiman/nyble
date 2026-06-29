function allocLoop(n) {
    var arr = [];
    var i = 0;
    while (i < n) {
        arr.push({a: 1, b: "hello", c: [1, 2, 3]});
        i = i + 1;
    }
    return arr.length;
}
var total = 0;
var j = 0;
while (j < 5) {
    total = total + allocLoop(500 + j * 100);
    j = j + 1;
}
console.log("stress: " + total);
console.log("GC test passed");
