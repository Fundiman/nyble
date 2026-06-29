// Zero-param arrow function
var f = () => 42;
console(f());

// Zero-param arrow with block body
var g = () => { return 99; };
console(g());

// Closure with capture
function makeCounter() {
    var count = 0;
    return () => { count = count + 1; return count; };
}
var counter = makeCounter();
console(counter());
console(counter());
console(counter());

// Member assignment
var obj = {x: 10};
obj.x = 20;
console(obj.x);

// Member computed assignment
obj["x"] = 30;
console(obj.x);

// Compound member assignment
obj.x = obj.x + 5;
console(obj.x);

// Nested member access
var obj2 = {inner: {value: 100}};
obj2.inner.value = 200;
console(obj2.inner.value);

// GC stress with many objects
var arr = [];
var i = 0;
while (i < 5000) {
    arr.push({a: i, b: "str_" + i});
    i = i + 1;
}
console("arr length: " + arr.length);

// Verify objects are intact
console(arr[0].a);
console(arr[4999].a);

console("all feature tests passed");
