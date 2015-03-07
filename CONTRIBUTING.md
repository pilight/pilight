Rules for contributing to pilight
=================================
First of all, thanks for making this project better. Without your help, pilight wouldn't have come to what it is now!

**BUT**, be aware that contributing to a big project shouldn't be considered a playground. It also isn't meant for you to learn `git`. So if you don't know what you are doing than don't do it. Instead open an [forum topic](http://forum.pilight.org), point to your code, and ask for help.

Checklist for opening an Issue
--------
When you think you found a bug make sure to first check these checklists. However, in every step, consider:

**If you are unsure, first open a topic on the forum!!!**

**Step 1: Version**

1. First check if you run the latest pilight version.
2. Check if the issue is solved in the latest nightly.
3. If it is solved, don't open an issue! If the bug is still there, continue with the next checklists.

**Step 2.1: Programming issues**

1. Your bug is a core programming issue like a `segfault` or similar.
2. The bug is not causes by any outdated or custom compiled modules.
3. Your bug is not caused by any alteration or third party patch.
4. Open an issue according to the Issue requirements.

**Step 2.2: Other issues**

1. Checking the pilight logs didn't give you any answer.
2. You RTFM (read the fucking manual), that means pilight website pages and wiki, and it didn't gave a solution.
2. You searched the forum for similar issues and their possible solution but didn't find one.
3. You are running on a platform (revision!) that is supported by pilight.
4. You are using official pilight distributers (don't use obscure repositories).
4. Open an issue according to the Issue requirements.

**Step 3: Issue Requirements**

1. Post your full configuration.
2. Post the running version of pilight `pilight-daemon -V`.
3. Post the content of pilight.err and pilight.log.
4. Post the content of pilight-daemon in not deamonized mode `--nodaemon` of when issue appeared.
5. Post what OS you are using pilight on (`uname -a`) and what version of the OS.
6. Post what revision of the platform you are using (e.g. Raspberry Pi A, B, B+ or 2 B). 
6. Clearly describe how the issue can be reproduced.
7. Format your issue with [Github markdown](https://help.github.com/articles/github-flavored-markdown/)
8. You are willing to follow-up on our comments.
9. You are willing to test our fixes.

If you are unsure about any of the above steps then **don't open an issue**.

Checklist for doing a Pull Request
---------
When you contribute to pilight make sure to follow this checklist:

1. Is your pull request directed at the development branch?
*pilight doesn't accept contributions to the master branch because this branch reflects the latest stable code.*
2. Did you read [development](http://www.pilight.org/development/modules/) pages of pilight?
*Even if you think you know all, please check and double check. There are always small syntax changes. Also check similar already existing modules for syntax.*
3. Keep the coding style in sync with that of pilight (see below).
4. First merge with the latest development code.
5. Make a difference branch for each new feature you want to commit.
6. Test how pull-requests work on your own test repositories.
7. Make sure your pull-request contains [one single commit](http://eli.thegreenplace.net/2014/02/19/squashing-github-pull-requests-into-a-single-commit).
8. Open a pull-request when you indeed want to contribute and follow-up on our comments. If you don't want to implemented our requested changes after reviewing your pull-request, don't bother opening one.
9. Re-read this file before every pull-request, because it will be updated regularly.
10. Don't forget to enjoy the appreciation of the end user!

Coding style
-----
- No unnecessary spaces
```
if ( 1 == 1 )
{
...
}
```
should become:
```
if(1 == 1) {
...
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
but preferable use for around max 50 characters:
```
int x = 0, long_variable_name = 0, a = 0;
```
- Variable defining order.
First start with `struct`, then specials types (`*_t`), then `char`, then `double` / `float` and end with `int`.
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
- Initialize your variables.
```
char *a = NULL;
char a[10];
memset(&a, 0, 10);
double a = 0.0;
int a = 0;
```
- User the `static` keyword for all variables and function only use in the single C file your module consists of.
- Always use tabs instead of spaces for inline markup.
