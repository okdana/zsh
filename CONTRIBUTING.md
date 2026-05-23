# Contributing to zsh

zsh is primarily developed via mailing list. Generally patches and bug reports
should be submitted to <zsh-workers@zsh.org>, though security issues should be
reported to <zsh-security@zsh.org> instead (see also `SECURITY.md`).

Another mailing list, <zsh-users@zsh.org>, is provided for usage questions and
general discussion about zsh.

<https://zsh.org/mla/> provides an HTML archive of the mailing lists as well as
information about how to subscribe and unsubscribe.

In addition to the mailing list, it is possible to submit patches via GitHub PR
at <https://github.com/zsh-users/zsh/>. However, since many of the core
developers don't follow GitHub closely, this is mostly intended for minor,
obvious improvements like completion-function updates and typo fixes. Patches
that fix serious bugs, implement new shell functionality, or make controversial
changes to existing behaviour should be submitted to the mailing list.

## AI policy

Please don't submit 'AI'-generated patches. You may use whatever tools you like
for research, discovery, etc, but patches themselves should be written,
understood, and tested by their contributors. If you did use AI tools to
discover a bug or suggest a fix, please disclose this when submitting your
patch so that reviewers understand the context.

Please don't solicit for AI tools and services. The zsh project has no interest
in badges, scanners, bots, and other integrations.

## General advice for submitting bug reports and patches

- When reporting or fixing a problem, please include instructions for
  replicating it with `zsh -f`. (This disables zshrc and other customisations
  which might taint the results.)

- Try to match the existing indentation style, etc when making changes.

- Where possible, keep white-space fixes and other non-consequential changes as
  separate commits/diffs.

- Follow `Etc/completion-style-guide` and/or `Etc/zsh-development-guide` where
  possible.

- You don't need to be subscribed to post to the mailing list. However, you may
  want to mention if you're not, so that others know to Cc: you on their
  replies.

- When sending a patch to the list, it's preferred to include it at the bottom
  of the e-mail body. However, the default settings of many mail clients will
  corrupt it. You must set the message format to plain text (not rich text or
  HTML). Make sure that the patch is not hard-wrapped and that tabs haven't been
  converted to spaces or vice versa.

  If you're not sure how to send the patch safely, you can include it as an
  attachment instead.

- Avoid 'top-posting' when replying to posts on the mailing list -- trim the
  quoted text down to just what's necessary for context and add your response to
  each point below it.

- If your patch is accepted, it will be committed using the name and e-mail
  address associated with your mailing-list post or your GitHub account. Please
  mention if you would like to use a different name/address.
