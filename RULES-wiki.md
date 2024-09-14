# Rules for the sox_ng wiki

The "master copy" of the sox_ng wiki lives on Codeberg.
You can browse it online at
[`http://codeberg.org/sox_ng/sox_ng/wiki`](http://codeberg.org/sox_ng/sox_ng/wiki)
and fetch a copy by
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
```
One usually clones it onto the `wiki` subdirectory of a clone of `sox_ng`.

Editing the clone and pushing the changes is preferred to using the web
interface because the WI adds CRLF to every line, making the git diffs
incomprehensible.

The command-line interface is the only way to add images and attachments.

## Local HTML version

In the `wiki` directory there is a script `makehtml.sh`. If you run it,
it creates `index.html` and an HTML page for each page of the wiki.
If there is a `Home.md` page, this will become `index.html`
as well as `Home.html`.

## Markdown style

In theory, we should use standard
[Extended Syntax](https://markdownguide.offshoot.io/extended-syntax),
with its obligatory blank lines around header lines,
four-space indentation of items in ordered and unordered lists
and all the rest.

In practice we use forgejo/github/gitlab Markdown because it
seems to work mostly, with a couple of extra rules so that
`makehtml.sh` produces similar output to what Forgejo does.

### Headers

Put a blank line either side of #Header lines for better readability
of the `.md` files.

### Internal wikilinks

Internal wikilinks should be written as `[Accounting](Accounting)`
instead of just `[Accounting]`, otherwise `makehtml.sh` gets them wrong.

### Lists

#### List indentation

Indent second-level lists and continuation lines by two spaces,
not four like the standard says
not only because that's how 120 issues are already formatted
but also because the typography of `.md` files is better.

`makehtml.sh` converts each pair of spaces in `.md` files to four
before feeding it to `multimarkdown` so that lists format correctly.

Double spaces other than at the start of a line currently get
converted to four at the moment. Issue #139.

#### Line breaks in list items

Line breaks inside list items should be done with a blank line,.

Instead of
```
* mansr's 2015 post says
  When I recently decided to take a closer look at the DSD phenomenon
```
you should write
```
* mansr's 2015 post says

  When I recently decided to take a closer look at the DSD phenomenon
```
though Forgejo renders it with a blank line between the two lines of text.

If you really need a plain like break instead of a paragraph break
you must use an inline `<BR>` with no newline on either side:
```
* mansr's 2015 post says<BR>When I recently decided to take a closer look at the DSD phenomenon
```
but a paragraph break is preferred to make the `.md` file more readable.

#### Code blocks

Enclose paragraphs of code with a line of three grave quotes
before and after them.

Markdown also allows code blocks indented with spaces
but the two-space-to-four-space conversion for lists
may mess up the indentation.
