function test() {
    return this.multiplier || 1;
}
let obj = {multiplier: 5, test: test};
console.log(obj.test());
