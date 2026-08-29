# Code style

## Core code style

- Class names use CamelCase. Method, function, and variable names use snake_case.
- Class data members have an `m_` prefix and are usually private. Struct data members have no prefix and are usually public.
- Use `void set_attribute(int value)` and `int attribute() const` for setters and getters; avoid a `get_` prefix. Follow the [Qt recommendations](https://wiki.qt.io/API_Design_Principles#Naming_Boolean_Getters,_Setters,_and_Properties) for boolean getters.
- Structs are usually small and simple, have few or no methods, and never use inheritance.
- Files use CamelCase when they contain a CamelCase class. Otherwise, files use snake_case and their contents belong to a matching snake_case namespace.
- Reflect the directory and file structure in namespaces. For example, `folder/structure.h` corresponds to `namespace folder::structure { ... }`.
- Indent with four spaces. Do not use tabs.
- Format new code, and only new code, with the repository's [`.clang-format`](../.clang-format) file. Do not reformat unrelated existing code.
- Follow the [Qt API design principles](https://wiki.qt.io/API_Design_Principles) and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) where this guide does not specify a rule.

## Avoid repeated type names

Avoid repeating the same name across namespaces and types. For example, `io::envelope::Envelope` is undesirable. A supporting type such as `io::envelope::Version` reads well, but placing the main class in that namespace forces its longer, repetitive name.

Prefer `io::Envelope`. Put supporting types either inside `io::Envelope` as public types or in an `io::envelope_details` namespace.

## Pointers vs. references

Avoid out parameters in new APIs when practical; prefer returning a small struct. When an out parameter is necessary, use a pointer rather than a non-const reference. The address-of operator at the call site makes it clear that the argument can be modified:

```cpp
color.get_hsv(&hue, &saturation, &value);
```

## The art of naming

Names are a central part of API design. Avoid abbreviations, even familiar ones, because consistent full words are easier to understand and remember. Choose precise names that preserve useful context without adding redundant words. If a concept is difficult to name or describe in one sentence, reconsider whether it belongs in the API.

Use consistent names for related classes and operations. A function name should make clear whether the function has side effects. Give parameters descriptive names and use the same names in declarations, definitions, and documentation. Avoid repeating an enum type's name in scoped enumerators when the scope already provides the context.

## Error handling

Functions that can fail return `Expected<T>`, which is an alias for `std::expected<T, Error>`.

### Checking results

Use the short boolean form in control flow:

```cpp
if (!result) {
    // Handle error.
}

if (result) {
    // Handle value.
}
```

### Creating errors

Use `Error::fail` where an error originates instead of `std::unexpected`:

```cpp
return Error::fail(
    Error::Code::InvalidInput,
    "invalid storage layout");
```

Choose the most specific error code available.

### Forwarding errors

Generally, errors can be forwarded with `Error::propagate`:

```cpp
auto result = operation(); // Expected<Source>
if (!result) {
    // result can be either a std::expected or an Error.
    return Error::propagate(std::move(result));
}
// The current function returns Expected<Target>.
```

`propagate` adds the source file and line as additional context. In some cases, it can be useful to avoid that:

```cpp
auto result = operation();
if (!result) {
    return result;
}
```

This is appropriate for transparent helpers, recursive calls, and simple delegation where another error frame would add no useful information. When the success types differ and direct return is impossible, use `propagate`.

### Adding context

At meaningful abstraction or operation boundaries, propagate with a concise context message:

```cpp
auto result = write_payload(path);
if (!result) {
    // result can be either a std::expected or an Error.
    return Error::propagate(std::move(result), "write DAG metadata");
}
```

Useful context identifies what the caller was trying to accomplish, such as:

- The domain operation: `"gather relevant input leaves"`.
- The data role: `"read DAG clustering"`.
- The affected node: `"add saved node " + key + " to storage index"`.
- The higher-level phase: `"finalize merged output dataset"`.

Avoid merely repeating details already supplied by the lower-level error. For example, if the lower layer records the exact path and I/O operation, the caller should add the file's semantic role rather than repeat the path unnecessarily.

The error type can be changed (reclassified) as well:

```cpp
auto result = write_payload(path);
if (!result) {
    return Error::propagate(
        std::move(result),
        Error::Code::CorruptData,
        "write DAG metadata");
}
```

Reclassify an existing error only if the current abstraction genuinely changes its meaning.
