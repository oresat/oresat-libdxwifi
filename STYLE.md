# Style
# # C guidelines

These guidelines are closely adapted from the ClusterLabs Pacemaker Development
C style guide:
https://clusterlabs.org/pacemaker/doc/2.1/Pacemaker\_Development/singlehtml/index.html

* Line formatting
  * Indentation must be 4 spaces, no tabs.
  * Do not leave trailing whitespace.
  * Lines should be no longer than 80 characters unless limiting line length
    hurts readability.

* Comments
```
/* Single-line comments may look like this */

// ... or this

/* Multi-line comments should start immediately after the comment opening.
 * Subsequent lines should start with an aligned asterisk. The comment
 * closing should be aligned and on a line by itself.
 */
```

* Control statements
```
/* (1) The control keyword is followed by a space, a left parenthesis without a
 *     space, the condition, a right parenthesis, a space, and the opening brace
 *     on the same line.
 * (2) Always use braces around control statement blocks, even if they contain
 *     only one line. This makes code review diffs smaller if a line gets added
 *     in the future, and it avoids the chance of bad indentation making a line
 *     incorrectly appear to be part of the block.
 * (3) The closing bracket is on a line by itself.
 */
if (v < 0) {
    return 0;
}

/* "else" and "else if" are on the same line with the previous ending brace and
 * next opening brace, separated by a space. Blank lines may be used between
 * blocks to help readability.
 */
if (v > 0) {
    return 0;

} else if (a == 0) {
    return 1;

} else {
    return 2;
}

/* Do not use assignments in conditions. This ensures that the developer's
 * intent is always clear, makes code reviews easier, and reduces the chance of
 * using assignment where comparison is intended.
 */
// Do this...
a = f();
if (a) {
    return 0;
}
// ...NOT this
if (a = f()) {
    return 0;
}

/* It helps readability to use the "!" operator only in boolean comparisons, and
 * to explicitly compare numeric values against 0, pointers against NULL, etc.
 * This helps remind the reader of the type being compared.
 */
int i = 0;
char *s = NULL;
bool cond = false;

if (!cond) {
    return 0;
}
if (i == 0) {
    return 0;
}
if (s == NULL) {
    return 0;
}

/* In a "switch" statement, indent "case" one level, and indent the body of each
 * "case" another level.
 */
switch (expression) {
    case 0:
        command1;
        break;
    case 1:
        command2;
        break;
    default:
        command3;
        break;
}
```

* Variables
  * Declaration and initialization
```
// Variables declarations occur only at the beginning of a block.

/* A variable is declared using the type, one space, and the name. Variable
 * names should not be aligned with one another unless doing so significantly
 * helps readability.
 */
// Do this...
int x;
long long y;

/// ...NOT this
int       x;
long long y;

/* A variable is initialized using the type, one space, the name, an equal sign,
 * and the value. Names, values, and equal signs should not be aligned with one
 * another unless doing so significantly helps readability (for example, in
 * enum definitions).
 */
// Do this...
int x = 5;
long long y = 10;

// ...NOT this
int x       = 5;
long long y = 10;
```

  * Pointers
```
/* (1) The asterisk goes next to the variable name, not next to the type.
 * (2) Avoid leaving pointers uninitialized, to lessen the impact of
 *     use-before-assignment bugs.
 */
char *my_string = NULL;

// Use space before asterisk and after closing parenthesis in a cast
char *foo = (char *) bar;
```

* Functions
```
/*
 * (1) The return type goes on its own line
 * (2) The opening brace goes by itself on a line
 * (3) Use "const" with pointer arguments whenever appropriate, to allow the
 *     function to be used by more callers.
 */
int
my_func1(const char *s)
{
    return 0;
}

/* Functions with no arguments must explicitly list them as void,
 * for compatibility with strict compilers
 */
int
my_func2(void)
{
    return 0;
}

/*
 * (1) For functions with enough arguments that they must break to the next
 *     line, align arguments with the first argument.
 * (2) When a function argument is a function itself, use the pointer form.
 * (3) Declare functions and file-global variables as ``static`` whenever
 *     appropriate. This gains a slight efficiency in shared libraries, and
 *     helps the reader know that it is not used outside the one file.
 */
static int
my_func3(int bar, const char *a, const char *b, const char *c,
         void (*callback)())
{
    return 0;
}
```
