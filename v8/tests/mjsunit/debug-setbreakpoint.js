// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple function which stores the last debug event.
listenerComplete = false;
exception = false;

var base_request = '"seq":0,"type":"request","command":"setbreakpoint"'

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testArguments(dcp, arguments, success, type) {
  var request = '{' + base_request + ',"arguments":' + arguments + '}'
  var json_response = dcp.processDebugJSONRequest(request);
  var response = safeEval(json_response);
  if (success) {
    assertTrue(response.success, json_response);
    assertEquals(type ? type : 'script', response.body.type, json_response);
  } else {
    assertFalse(response.success, json_response);
  }
}

function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {
    // Get the debug command processor.
    var dcp = exec_state.debugCommandProcessor();

    // Test some illegal setbreakpoint requests.
    var request = '{' + base_request + '}'
    var response = safeEval(dcp.processDebugJSONRequest(request));
    assertFalse(response.success);

    testArguments(dcp, '{}', false);
    testArguments(dcp, '{"type":"xx"}', false);
    testArguments(dcp, '{"type":"function"}', false);
    testArguments(dcp, '{"type":"script"}', false);
    testArguments(dcp, '{"target":"f"}', false);
    testArguments(dcp, '{"type":"xx","target":"xx"}', false);
    testArguments(dcp, '{"type":"function","target":1}', false);
    testArguments(dcp, '{"type":"function","target":"f","line":-1}', false);
    testArguments(dcp, '{"type":"function","target":"f","column":-1}', false);
    testArguments(dcp, '{"type":"function","target":"f","ignoreCount":-1}', false);

    // Test some legal setbreakpoint requests.
    testArguments(dcp, '{"type":"function","target":"f"}', true);
    testArguments(dcp, '{"type":"function","target":"h"}', true, 'function');
    testArguments(dcp, '{"type":"function","target":"f","line":1}', true);
    testArguments(dcp, '{"type":"function","target":"f","position":1}', true);
    testArguments(dcp, '{"type":"function","target":"f","condition":"i == 1"}', true);
    testArguments(dcp, '{"type":"function","target":"f","enabled":true}', true);
    testArguments(dcp, '{"type":"function","target":"f","enabled":false}', true);
    testArguments(dcp, '{"type":"function","target":"f","ignoreCount":7}', true);
    testArguments(dcp, '{"type":"script","target":"test"}', true);
    testArguments(dcp, '{"type":"script","target":"test"}', true);
    testArguments(dcp, '{"type":"script","target":"test","line":1}', true);
    testArguments(dcp, '{"type":"script","target":"test","column":1}', true);

    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.addListener(listener);

function f() {
  a=1
};

function g() {
  f();
};

eval('function h(){}');

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(g, 0, 0);
g();

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
assertFalse(exception, "exception in listener")
