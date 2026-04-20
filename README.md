# C Plagiarism Detection System

C source code plagiarism detector that analyzes code using  Lexical tokens, Syntax Trees, and Control Flow Graphs.




  - **Lexical Layer**: Tokenizes source code using a dedicated `LexicalScanner`. It removes comments/whitespace and utilizes the **Winnowing algorithm** for fingerprinting.
  - **Structural Layer (AST)**: Builds an Abstract Syntax Tree to compare code structure. It ignores variable names and types to detect renaming.
  - **Control Flow Layer (CFG)**: Constructs Control Flow Graphs to identify structural logic similarities, remaining effective even when statements are reordered or loop types are changed.
- **Semantic Normalization**:
  - Automatically normalizes compound assignments (e.g., `a += b` is treated as `a = a + b`) during parsing.
  - Handles various loop types (`for`, `while`) and conditional structures (`if`/`else`).
- **Winnowing Algorithm**: Generates fingerprints for each file, enabling scalable and robust comparison that is resistant to minor modifications.



### Compilation

Compile the system :

```bash
gcc plag_checker.c -o checker
```

## Usage

You must provide exactly two C source files to compare.

### Running a Comparison
```bash
./checker test/file1.c test/file2.c
```

### Example Output
```text
Comparing test/file1.c and test/file2.c:
  Lexical Token Score:  100.00%
  Structural AST Score: 100.00%
  Control Flow CFG Score: 100.00%
  Overall Final Similarity: 100.00%
  Identified Matching Regions:
    File 1 lines 1-1 matched File 2 lines 1-1
    File 1 lines 2-3 matched File 2 lines 2-3
    ...
  Analysis Confidence: HIGH
--------------------------------------
```



