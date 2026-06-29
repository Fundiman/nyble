// Large file to trigger VM path
let a0=0;let a1=1;let a2=2;let a3=3;let a4=4;let a5=5;let a6=6;let a7=7;let a8=8;let a9=9;
let b0=0;let b1=1;let b2=2;let b3=3;let b4=4;let b5=5;let b6=6;let b7=7;let b8=8;let b9=9;
let c0=0;let c1=1;let c2=2;let c3=3;let c4=4;let c5=5;let c6=6;let c7=7;let c8=8;let c9=9;
let d0=0;let d1=1;let d2=2;let d3=3;let d4=4;let d5=5;let d6=6;let d7=7;let d8=8;let d9=9;
let e0=0;let e1=1;let e2=2;let e3=3;let e4=4;let e5=5;let e6=6;let e7=7;let e8=8;let e9=9;
let sum=0;
sum=sum+a0;sum=sum+a1;sum=sum+a2;sum=sum+a3;sum=sum+a4;sum=sum+a5;sum=sum+a6;sum=sum+a7;sum=sum+a8;sum=sum+a9;
sum=sum+b0;sum=sum+b1;sum=sum+b2;sum=sum+b3;sum=sum+b4;sum=sum+b5;sum=sum+b6;sum=sum+b7;sum=sum+b8;sum=sum+b9;
sum=sum+c0;sum=sum+c1;sum=sum+c2;sum=sum+c3;sum=sum+c4;sum=sum+c5;sum=sum+c6;sum=sum+c7;sum=sum+c8;sum=sum+c9;
sum=sum+d0;sum=sum+d1;sum=sum+d2;sum=sum+d3;sum=sum+d4;sum=sum+d5;sum=sum+d6;sum=sum+d7;sum=sum+d8;sum=sum+d9;
sum=sum+e0;sum=sum+e1;sum=sum+e2;sum=sum+e3;sum=sum+e4;sum=sum+e5;sum=sum+e6;sum=sum+e7;sum=sum+e8;sum=sum+e9;
console.log("pre-sum:", sum);

// Test try/catch in VM path
let caught = false;
try {
    throw "vm_error";
} catch (err) {
    console.log("VM caught:", err);
    caught = true;
}
console.log("VM catch test:", caught ? "PASS" : "FAIL");

// Test finally in VM path
let fin = false;
try {
    console.log("VM try");
} finally {
    fin = true;
    console.log("VM finally");
}
console.log("VM finally test:", fin ? "PASS" : "FAIL");

// Test nested try/catch/finally in VM path
try {
    try {
        throw "nested_vm";
    } catch (inner) {
        console.log("VM inner:", inner);
        throw "rethrow_vm";
    } finally {
        console.log("VM inner finally");
    }
} catch (outer) {
    console.log("VM outer:", outer);
}
console.log("All VM tests done");
