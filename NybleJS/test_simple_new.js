// Simple new test
function Foo(x) {
    this.val = x;
}
let f = new Foo(42);
console.log(f.val);

// Prototype test
console.log(typeof Foo.prototype);
console.log(typeof Foo.prototype.constructor);

// This binding in method
let obj = {
    name: "test",
    greet: function() {
        return this.name;
    }
};
console.log(obj.greet());
