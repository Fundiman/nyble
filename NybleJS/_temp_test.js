var arr = [];
var i = 0;
while (i < 100) {
    arr.push({a: i, b: "str_" + i});
    i = i + 1;
}
console("done " + arr.length);

var arr2 = [];
var j = 0;
while (j < 5000) {
    arr2.push({a: j, b: "str_" + j});
    j = j + 1;
}
console("done2 " + arr2.length);
