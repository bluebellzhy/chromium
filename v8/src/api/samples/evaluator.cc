#include <public/v8.h>
#include <cstring>
#include <cstdio>


void RunShell(v8::Handle<v8::Context> context);
bool ExecuteString(v8::Handle<v8::String> source);
v8::Handle<v8::Value> Print(const v8::Arguments &args);
v8::Handle<v8::String> ReadFile(const char* name);


int main(int argc, char* argv[]) {
  v8::HandleScope handle_scope;
  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  // Bind the global 'print' function to the C++ Print callback.
  global->Set(v8::String::New("print"), v8::FunctionTemplate::New(Print));
  // Create a new execution environment containing the 'print' function.
  v8::Handle<v8::Context> context = v8::Context::New(NULL, global);
  // Enter the newly created execution environment.
  v8::Context::Scope context_scope(context);
  bool run_shell = (argc == 1);
  for (int i = 1; i < argc; i++) {
    const char* str = argv[i];
    if (strcmp(str, "--shell") == 0) {
      run_shell = true;
    } else {
      v8::HandleScope handle_scope;
      v8::Handle<v8::String> source = ReadFile(str);
      if (source.IsEmpty()) {
        printf("Error reading '%s'\n", str);
        return 1;
      }
      if (!ExecuteString(source))
        return 1;
    }
  }
  if (run_shell) RunShell(context);
  return 0;
}


// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
v8::Handle<v8::Value> Print(const v8::Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    if (first) first = false;
    else printf(" ");
    v8::String::AsciiValue str(args[i]);
    printf("%s", *str);
  }
  printf("\n");
  return v8::Undefined();
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size; ) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}


// The read-eval-execute loop of the shell.
void RunShell(v8::Handle<v8::Context> context) {
  static const int kBufferSize = 256;
  while (true) {
    char buffer[kBufferSize];
    printf("> ");
    char* str = fgets(buffer, kBufferSize, stdin);
    if (str == NULL) break;
    v8::HandleScope handle_scope;
    ExecuteString(v8::String::New(str));
  }
  printf("\n");
}


// Executes a string within the current v8 context.
bool ExecuteString(v8::Handle<v8::String> source) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    v8::String::AsciiValue error(try_catch.Exception());
    printf("%s\n", *error);
    return false;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      // Print errors that happened during execution.
      v8::String::AsciiValue error(try_catch.Exception());
      printf("%s\n", *error);
      return false;
    } else {
      if (!result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::AsciiValue str(result);
        printf("%s\n", *str);
      }
      return true;
    }
  }
}
