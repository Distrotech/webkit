description(
'Tests whether bytecode codegen properly handles temporaries.'
);

var a = true;
a = false || a;
shouldBeTrue("a");

var b = false;
b = true && b;
shouldBeFalse("b");

successfullyParsed = true;
