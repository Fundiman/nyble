# Nyble — a web browser built from scratch
Nyble is a web browser built from scratch designed for modern websites — think of Discord or Youtube.

Most browsers need literal super computers to compile; Nyble does not.

> [!NOTE]
> This is build/libant-ci — this branch was made to build libant since we're throwing out Nyble's JS engine for `libant`. Well we're still thinking about it and it's not the final decision.

# Frequently Asked Questions (FAQ)

<details>
  <summary><b>1. Why do I only see a JavaScript engine?</b></summary>
  <br>
    
   We don't want to work on EVERYTHING at once—we'll complete ONE component and then move on to the next. 
</details>

<details>
  <summary><b>2. How do I build this project?</b></summary>
  <br>

  Each component SHOULD have a Makefile — run `make` to build it. 
</details>

<details>
  <summary><b>3. When will this be ready for like, normal daily usage?</b></summary>
  <br>

  Probably in a few months if not years. [AntJS](https://github.com/themackabu/ant) has helped us speed up progress significantly — there's still plenty of time left, but not an eternity unlike before.
</details>

<details>
  <summary><b>4. WHY IS THE REPOSITORY USING ZIG THAT'S BLOAT JUST LIKE RUST I CAN'T GET THAT ON MY PHONE!!</b></summary>
  <br>

  You should be grateful that I didn't use Rust for this project because I despise it (it's compile-time bloat). 
  
  In all seriousness, I really didn't want to use another compiler other than gcc/g++ (MinGW-w64) but I had to because of AntJS compatibility and stuff — if you can't get Zig on you device, you could:
  - either write patches to make it work with whatever compiler you have;
  - OR use GitHub Actions (fork the repository, push your changes, and run the workflow).
</details>

# Licensing
This project is licensed under [MPL-2.0](https://www.mozilla.org/en-US/MPL/2.0), click [here for the repository's snapshot of the LICENSE](LICENSE).
