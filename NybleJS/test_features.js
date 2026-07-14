// Zero-param arrow function
var f = () => 42;
console.log(f());

// Zero-param arrow with block body
var g = () => { return 99; };
console.log(g());

// Closure with capture
function makeCounter() {
    var count = 0;
    return () => { count = count + 1; return count; };
}
var counter = makeCounter();
console.log(counter());
console.log(counter());
console.log(counter());

// Member assignment
var obj = {x: 10};
obj.x = 20;
console.log(obj.x);

// Member computed assignment
obj["x"] = 30;
console.log(obj.x);

// Compound member assignment
obj.x = obj.x + 5;
console.log(obj.x);

// Nested member access
var obj2 = {inner: {value: 100}};
obj2.inner.value = 200;
console.log(obj2.inner.value);

// GC stress with many objects
var arr = [];
var i = 0;
while (i < 5000) {
    arr.push({a: i, b: "str_" + i});
    i = i + 1;
}
console.log("arr length: " + arr.length);

// Verify objects are intact
console.log(arr[0].a);
console.log(arr[4999].a);

console.log("all feature tests passed");
