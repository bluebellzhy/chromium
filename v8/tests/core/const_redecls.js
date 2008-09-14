/*
const semantics
---------------

1) The first encountered declaration of a name determines it's mode (var or const).
(Note that all "static" (i.e. source text) declarations are "executed" at function
entry, before any of the statements of the function are executed. That is, if there
is a static const declaration at the end of the function, it still precedes any
dynamic const declaration (via eval()) that may be encountered before.)

2) A const name cannot be redeclared. Redeclaration leads to a type error exception.
(In particular, a const name cannot be redeclared as const, even if the initial
expression is the same). The type error may be thrown at compile/parse time.

3) const names follow the same scoping rules as functions.
(In particular, they are subtly different from variables. For example:

  var obj = { a: 0, b: 1 };
  with (obj) {
    const a = 7;
    var b = 8;
  }
  // a == 7, obj.a == 0
  // b == undefined, obj.b == 8 (because "var v = x" => "var v; v = x")
)

4) The first executed initialization expression sets the value of a const; the
value is undefined before that. Example:

  function () {
    use(a);  // a is undefined
    const a = 7;
    use(a);  // a is 7
  }
  
  function () {
    use(a);  // a is undefined
    for (var i = -10; i < 10; i++) {
      const a = i;
      use(a);  // a is always -10
    }
    use(a);  // a is still -10
  }
  
Note that empty initialization (missing initialization expression) is legal
and initializes the name to undefined.

5) Assignments to const names are ignored.
*/


function Test(s) {
  try {
    eval("function () {" + s + "}")();
    throw "expected declaration error";
  } catch (x) {
    var e = x.toString();
    if (// look for correct smjs error
      (e.indexOf("TypeError") >= 0 && e.indexOf("redeclaration") >= 0) ||
      // or correct V8 (parse-time) syntax error (we should really have a TypeError here...)
      (e.indexOf("SyntaxError") >= 0 && e.indexOf("has already been declared") >= 0) ||
      // or correct V8 (run-time) type error
      (e.indexOf("TypeError") >= 0 && e.indexOf("has already been declared") >= 0)
    ) {
      return;  // we are ok
    }
    print("Test \"" + s + "\" failed with exception: " + x);
  }
}


// Sanity check
Test(
  "var a;"
);


// Sanity check
Test(
  "throw 0;"
);


// ----------------------------------------------------------------------------
// Tests

Test(
  "const a = 0;" +
  "const a = 1;"
);


Test(
  "const a = undefined;" +
  "const a = 7;"
);


Test(
  "const a;" +
  "const a = 7;"
);


Test(
  "const a = 0;" +
  "var a = 1;"
);


Test(
  "const a = 0;" +
  "function a() {};"
);


Test(
  "function a() {};" +
  "const a = 0;"
);


Test(
  "var a;" +
  "const a;"
);


Test(
  "var a = 0;" +
  "const a = 0;"
);


Test(
  "const a = 0;" +
  "eval(\"const a = 1\");"
);


Test(
  "const a;" +
  "eval(\"var a;\");"
);


Test(
  "const a;" +
  "eval(\"function a() {}\");"
);


Test(
  "var a;" +
  "eval(\"const a;\");"
);


Test(
  "function a() {};" +
  "eval(\"const a;\");"
);


Test(
  "eval(\"const a;\");" +
  "eval(\"const a;\");"
);


Test(
  "eval(\"var a;\");" +
  "eval(\"const a;\");"
);


Test(
  "eval(\"function a() {};\");" +
  "eval(\"const a;\");"
);


Test(
  "eval(\"const a;\");" +
  "eval(\"var a;\");"
);


Test(
  "eval(\"const a;\");" +
  "eval(\"function a() {};\");"
);
