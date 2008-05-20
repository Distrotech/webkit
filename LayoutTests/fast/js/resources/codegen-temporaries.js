description(
'Tests whether bytecode codegen properly handles temporaries.'
);

var a = true;
a = false || a;
shouldBeTrue("a");

var b = false;
b = true && b;
shouldBeFalse("b");

function TestObject() {
    this.toString = function() { return this.test; }
    this.test = "FAIL";
    return this;
}

function assign_test1()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test1()", "'PASS'");

function assign_test2()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test2()", "'PASS'");

function assign_test3()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test3()", "'PASS'");

var testObject4 = new TestObject;
var a4 = testObject4;
a4.test = this.a4 = "PASS";

shouldBe("testObject4.test", "'PASS'");

var testObject5 = new TestObject;
var a5 = testObject5;
a5 = this.a5.test = "PASS";

shouldBe("testObject5.test", "'PASS'");

function assign_test6()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test6()", "'PASS'");

function assign_test7()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test7()", "'PASS'");

function assign_test8()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test8()", "'PASS'");

function assign_test9()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = this.a = "PASS";
    return testObject.test;
}

shouldBe("assign_test9()", "'PASS'");

var testObject10 = new TestObject;
var a10 = testObject10;
a10 = this.a10["test"] = "PASS";

shouldBe("testObject10.test", "'PASS'");

function assign_test11()
{
    var testObject = new TestObject;
    var a = testObject;
    a[a = "test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test11()", "'PASS'");

function assign_test12()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = "PASS";
    return testObject.test;
}

shouldBe("assign_test12()", "'PASS'");

function assign_test13()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test13()", "'PASS'");

function assign_test14()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test14()", "'PASS'");

function assign_test15()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = (test = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test15()", "'PASS'");

function assign_test16()
{
    var a = 1;
    a = (a = 2);
    return a;
}

shouldBe("assign_test16()", "2");

successfullyParsed = true;
