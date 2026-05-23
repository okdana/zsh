# zsh security policy

Please report security issues to: <zsh-security@zsh.org>

This is a mailing list whose subscribers are limited to zsh's core developers
and a few other trusted users.

Note that the project generally does not treat the following as security
vulnerabilities:

- Crashes, hangs, memory corruption, or memory leaks that can only be triggered
  by a user choosing to execute a command or script.

- DoS or command execution that relies on an attacker modifying a script or .zwc
  file. (Anyone who can modify a script the shell executes can necessarily
  control the shell.)

- Similarly, anything that relies on an attacker having super-user privileges
  (legitimately or not)

- Naturally, anything involving code that is not shipped with the official zsh
  distribution. This includes third-party 'frameworks' and 'plug-ins', as well
  as modifications made by down-stream packagers

However, there are some exceptions, such as a command that leaks sensitive data
to a world-readable file or a command that can be tricked into executing code by
reading a file that isn't meant to be interpreted as such.

If in doubt, feel free to e-mail the `-security` list anyway.

The zsh project does not have a bug-bounty programme.

See `CONTRIBUTING.md` for general information about submitting patches.
