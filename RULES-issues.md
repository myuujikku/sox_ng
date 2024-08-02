# Rules for sox_ng issues

The master copy of the issues lives on Codeberg.

You can make a local copy of them into the `issues` subdirectory
where there is a script `getissues.sh` that fetches each open issue
into an `.md` file named according to the issue's title
containing its initial description and into a directory of the same name
without the `.md` suffix containing attachments and
- if the issue has labels, they are listed one per line in a file `labels`
- if the issue has a milestone, a file `milestone` containing
  `micro`, `minor` or `major`. The milestones are used instead of
  labels `bug` or `enhancement`.
- a file `number` contining the issue number.

Comments are not currently fetched. Maybe they should be. #34
Attachments are not currently fetched. They should be. #35

Attachments to issues are not currently migrated if you
migrate the codeberg repository to another Forgejo instance.

In future, the master copy of the issue database will live in the
source tree and the web version will be a copy of it. #80
Like the wiki, there will be a script to make HTML pages of it. #81
