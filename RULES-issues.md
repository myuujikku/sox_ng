# Rules for sox_ng issues
## Usage
The master copy of the issues lives on Codeberg.

You can make a local copy of them into the `issues` subdirectory
where there is a script `getissues.sh` that fetches each open issue
into an `.md` file named according to the issue's title,
containing its initial description and into a directory of the same name
without the `.md` suffix for its metadata and attachments.

Attachments to issues are not currently migrated if you
migrate the codeberg repository to another Forgejo instance.
[codeberg.org/forgejo/forgejo issue 4787](https://codeberg.org/forgejo/forgejo/issues/4787)

In future, the master copy of the issue database will live in the
source tree and the web version will be a copy of it. #80

Like the wiki, it has a script `makehtml.sh` to make HTML pages of it.

## Format of an issue
### Title
A one-liner, as short as possible.

### Description
The first line of every issue is # Title`, the same as the Title:

The second-level headings are normally
`## Links``, `## Repeat by``, `## Analysis` and `## Conclusion`.

### Attachments
Test files and patches are attached to the description.

### Comments
Issue comments are not kept in the source tree.

If people add them to the web versiobn, other webby people can
edit their wisdom into the main description;
that way our serious command-line friends will get the best version
of the problem description and be spared wading through chitchat.

- if the issue has a milestone, a file `milestone` containing
  `micro`, `minor`, `major`, `release` or `null` if it has no milestone.
  The milestones are used instead of labels `bug` or `enhancement`.
- a file `number` contining its issue number.
### Milestone
Kept in a file `miletone`, one of:
* `micro` for bug fixes
* `minor` for enhancements
* `major` for non backward-compatible changes
* `release` if it regards the SoX_ng project's infrastructure
  and should be included in the next release

### Labels
Kept in a file `labels`, one per line, all optional:
* `patch`: Patches are included
* `copyright`: The issue impacts on `sox_ng`'s copyright status.
* `needswork`: All info seems to be in but work is needed.
* `duplicate`: This report has the same cause as another issue..
  A duplicate issue's title (not its first line) should end `=#35`
  to say which is the page for this problem and so that where to go
  appears for duplicates in the summary list of issues.
* `unconfirmed`: We have heard of a bug but not seen if it bites us yet.

### Open or Closed
Kept in a file `state` containing `open` or `closed`.

### Issue number
Kept in a file `number` in decimal.

Issue numbers, how 1960s,
and showing them by default in reverse order of submission,
with the newewst and most noisy first, is just odd.

Random numbers would be better; that way individuals can create
issues offline with vanishing risk of issue number clashes and
if you really want to see them sorted by most recent activity,
there's `ls -t`.

I wonder what the word size of an issue number is in `forgejo`.

### Anything else
Mo, but submitter and submission date would probably be useful.
