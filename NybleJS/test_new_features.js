// Test 1: Basic prototype chain
function Animal(name) {
    this.name = name;
}
Animal.prototype.speak = function() {
    return this.name + " says hi";
};

let a = new Animal("Rex");
console.log(a.name);
console.log(a.speak());

// Test 2: Inheritance via prototype chain
function Dog(name, breed) {
    this.name = name;
    this.breed = breed;
}
Dog.prototype = new Animal("");
Dog.prototype.bark = function() {
    return this.name + " barks!";
};

let d = new Dog("Buddy", "Golden");
console.log(d.name);
console.log(d.breed);
console.log(d.speak());
console.log(d.bark());

// Test 3: this binding in method calls
let obj = {
    value: 42,
    getValue: function() {
        return this.value;
    }
};
console.log(obj.getValue());

// Test 4: Constructor returning object
function Special() {
    this.x = 10;
    return { custom: true };
}
let s = new Special();
console.log(s.custom);
console.log(s.x);

// Test 5: Function.prototype.call
function greet(greeting) {
    return greeting + ", " + this.name;
}
console.log(greet.call({name: "World"}, "Hello"));
console.log(greet.call(a, "Hey"));

// Test 6: Function.prototype.apply
console.log(greet.apply({name: "Apply"}, ["Hi"]));

// Test 7: Function.prototype.bind
function add(a, b) {
    return (this.multiplier || 1) * (a + b);
}
let boundAdd = add.bind({multiplier: 10}, 2);
console.log(boundAdd(3));
