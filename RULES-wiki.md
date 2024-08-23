# Rules for the sox_ng wiki

The "master copy" of the sox_ng wiki lives on Codeberg.

You can browse it online at `https://codeberg.org/sox_ng/sox_ng/wiki`
and fetch a copy by
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
```
One usually clones it onto the `wiki` subdirectory of a clone of `sox_ng`.

The command-line interface is the only way to add images and attachments
to the wiki.

## HTML version

In the `wiki` directory there is a script `makehtml.sh`. If you run it,
it creates `index.html` and an HTML page for each page of the wiki.
If there is a `Home.md` page, this will become `index.html`
as well as `Home.html`.

## Markdown style
### Internal wikilinks
For the HTML encoding to work properly.
internal wikilinks should be written as `[Accounting](Accounting)`
instead of just `[Accounting]`.

### Lists
For `makehtml.sh` (i.e. `multimarkdown`) to render ordered and unordered lists
the same way as Forgejo, github and gitlab do, line breaks inside lists
should be done with a blank line, which starts a new paragraph.

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
If you really need a plain like break instead of a paragraph break
you must use an inline `<BR>` with no newline:
```
* mansr's 2015 post says<BR>When I recently decided to take a closer look at the DSD phenomenon
```
but a paragraph break is preferred so that the `.md` file
is as readable as the rendered output.
