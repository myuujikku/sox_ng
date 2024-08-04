# Rules for sox_ng issues

The master copy of the issues lives on Codeberg.

You can make a local copy of them into the `issues` subdirectory
where there is a script `getissues.sh` that fetches each open issue
into an `.md` file named according to the issue's title
containing its initial description and into a directory of the same name
without the `.md` suffix containing attachments and
- if the issue has labels, they are listed one per line in a file `labels`
- if the issue has a milestone, a file `milestone` containing
  `micro`, `minor`, `major`, `release` or `null` if it has no milestone.
  The milestones are used instead of labels `bug` or `enhancement`.
- a file `number` contining its issue number.

Comments are not currently fetched. #34

Attachments to issues are not currently migrated if you
migrate the codeberg repository to another Forgejo instance.
[codeberg.org/forgejo/forgejo issue 4787](https://codeberg.org/forgejo/forgejo/issues/4787)

In future, the master copy of the issue database will live in the
source tree and the web version will be a copy of it. #80

Like the wiki, it has a script `makehtml.sh` to make HTML pages of it.
