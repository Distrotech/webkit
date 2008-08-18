description(
"This test checks the behaviour of document.bindingDocuments as specified by the XBL specification."
);

debug("Initial value of bindingDocuments");
debug("");

var bindingDocuments = document.bindingDocuments;
shouldBeEqualToString("bindingDocuments.toString()", "[object NamedNodeMap]");
shouldBe("bindingDocuments.length", "0");
shouldBeNull("bindingDocuments.getNamedItem('foobar')");
shouldBeNull("bindingDocuments.item(0)");

debug("");
debug("Try changing the empty bindingDocuments");
debug("");

shouldThrow("bindingDocuments.setNamedItem(bindingDocuments.item(0))", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.setNamedItemNS(bindingDocuments.item(0))", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.removeNamedItem('foobar')", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.removeNamedItemNS('', 'foobar')", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');

debug("");
debug("Add a value to bindingDocuments and check the results (This part should fail as the loading code is not implemented)");
debug("");

document.loadBindingDocument("resources/xbl-passed-binding.xbl");

shouldBe("bindingDocuments.length", "1");
shouldBeEqualToString("bindingDocuments.item(0).toString()", "[object Document]");
shouldBeTrue("bindingDocuments.getNamedItem('resources/xbl-passed-binding.xbl')", "bindingDocuments.item(0)");
shouldBe("bindingDocuments.getNamedItemNS('', 'resources/xbl-passed-binding.xbl')", "bindingDocuments.item(0)");
shouldBe("bindingDocuments.getNamedItemNS('', 'resources/xbl-passed-binding.xbl')", "bindingDocuments.item(0)");
shouldBeNull("bindingDocuments.getNamedItemNS('foobar', 'resources/xbl-passed-binding.xbl')");

debug("");
debug("Try to change an existing value");
debug("");

shouldThrow("bindingDocuments.setNamedItem(bindingDocuments.item(0))", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.setNamedItemNS(bindingDocuments.item(0))", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.removeNamedItem('resources/xbl-passed-binding.xbl')", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
shouldThrow("bindingDocuments.removeNamedItemNS('', 'resources/xbl-passed-binding.xbl')", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');

successfullyParsed = true;
