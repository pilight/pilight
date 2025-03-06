Rules for contributing to pilight
=================================
First of all, thanks for making this project better. Without your help, pilight
wouldn't have become what it is now!

**BUT**, be aware that contributing to a big project shouldn't be considered a 
playground. It also isn't meant for you to learn `git`.
So if you don't know what you are doing, don't do it. Instead, open a
[forum topic](http://forum.pilight.org), point to your code and ask for help.

Checklist
---------
When you contribute to pilight make sure to follow this checklist:

1. Is your pull request directed at the development branch?
   *pilight doesn't accept contributions to the master branch because this 
   branch always contains the latest stable release.*
1. Make sure you use the code style specified below.
1. First merge with the latest development code.
1. Make a new branch for each new feature you want to commit.
1. Test how pull-requests work on your own test repositories first.
1. Make sure your pull-request contains [one single commit][squashing commits].
1. Open a pull-request when you indeed want to contribute and follow up on our 
   comments. If you don't want to implement our requested changes after 
   reviewing your pull-request, don't bother opening one.
1. Re-read this file before every pull-request, because it may have been 
   updated.
1. Don't forget to enjoy the appreciation of the end user!

[squashing commits]:http://eli.thegreenplace.net/2014/02/19/squashing-github-pull-requests-into-a-single-commit

Coding style
------------
pilight uses the [Artistic Style](http://astyle.sourceforge.net) tool to
enforce code style policies to ensure that the code style remains consistent
throughout our codebase. The following rules apply to all C code in the pilight
repository. The following rules apply to all C code in the pilight repository.

To make it easier to work with the code style guidelines, there is a 
[git pre-commit hook](ci/pre-commit) in ci/pre-commit which can be used to run
the same linter that CI uses whenever you run `git commit`. Note that you need 
to have Artistic Style installed for this to work.
Further installation instructions can be found in the file.

- Use 2 (two) spaces for indentation. Continuation lines should be aligned:
```
if(foo() == 0) {
  if(long_function_name_1() == 0 || long_function_name_2() == 0 ||
     long_function_name_3() == 0 || long_function_name_4() == 0) {
    return 0;
    }
  } else {
    return 1;
  }
}
```

- Lines should not exceed 80 characters. Long lines should be broken at <= 80
  characters length. Up to 90 characters may be used if doing so enhances
  readability.

- Braces must always be used, also for one-line blocks. Braces should be 
  attached to the previous line
```
int foo(bool isBar) {
    if (isBar) {
        bar();
        return 1;
    } else {
        return 0;
    }
}
```

- Pointer and reference operators must be attached to the name of the pointer or
  reference
```
int *foo;
char &bar;
```

- Indentation
  - case statements in switch blocks should be indented by one level
    ```
    switch(foo) {
      case 1:
        a += 1;
        break;
      case 2:
        a += 2;
        break;
    }
    ```
  - preprocessor blocks should be syntactically indented
    ```
    #ifdef WEBSERVER
      #ifdef WEBSERVER_HTTPS
        #include <mbedtls/md5.h>
      #endif
      #include "libs/pilight/core/webserver.h"
    #endif
    ```

- No unnecessary spaces or blank lines
```
if ( 1 == 1 ) {
  // code

  return 0;
}
```
should become:
```
if(1 == 1) {
  //code
  return 0;
}
```

- Don't inline variables:
```
int x                  = 0;
int long_variable_name = 0;
int a                  = 0;
```
but use
```
int x =	0;
int long_variable_name = 0;
int a = 0;
```
but preferably use:
```
int x = 0, long_variable_name = 0, a = 0;
```

- Variable defining order.
  First start with `struct`, then specials types (`*_t`), then `char`, then 
  `double` / `float`, and end with `int`
```
	struct protocol_threads_t *node = (struct protocol_threads_t *)param;
	struct JsonNode *json = (struct JsonNode *)node->param;
	struct JsonNode *jid = NULL;
	struct JsonNode *jchild = NULL;
	time_t time;
	char *tmp = NULL;
	double itmp = 0.0;
	int id = 0, state = 0, nstate = 0;
```

- Initialize all variables.
```
char *a = NULL;
char a[10];
memset(&a, 0, 10);
double a = 0.0;
int a = 0;
```

- User the `static` keyword for all variables and function only used in the 
  single C file your module consists of.


