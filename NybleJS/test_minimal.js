function Foo(x) {
    this.val = x;
}
let f = new Foo(42);
console.log(f.val);
