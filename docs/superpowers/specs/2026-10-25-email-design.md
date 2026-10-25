# Email on the notebook page — design

Status: agreed in conversation with the user on 2026-10-25, to be built in iterations.

## Intent

The user writes "check my email" (or similar) on a reMarkable page and taps the Claude button. Claude reads their unread Gmail, leaves out newsletters and noise, and handwrites onto the page only what needs their attention, one numbered item per email. They then answer on the page in their own handwriting, sees the finished reply emails, and sends them all with one tap. The tablet does this on its own, without their computer or phone.

They already have a recurring Claude Desktop job that triages email this way, and it works fine. The tablet version brings that onto paper, plus replying.

## Goals

1. **Check:** a handwritten request of any wording ("check my email", "anything in my inbox?") is routed to an email check.
2. **Triage:** every unread message in the Gmail inbox is considered. Only what needs the user is summarised. The rest is counted on one line ("Skipped 14: newsletters, receipts, notifications").
3. **Layout:** each important email is its own numbered paragraph or short bullet group, never one block of text. Each shows the sender, what they want, and any deadline.
4. **Mark as read:** after a successful check, every message the check looked at is marked read in Gmail, including skipped ones.
5. **Reply on paper:** the user can reply to several emails in one go, in any of three ways:
   - `reply 2: sounds good, Thursday works`
   - a hand-drawn line from an email summary to their note
   - a note written directly under a summary
6. **Drafts before sending:** Claude turns their notes into proper emails in their voice and inserts them on the page, numbered to match. Each draft shows which email it answers, the full To and Cc list, the subject, and the body.
7. **Reply-all:** replies go to all original recipients by default. the user checks the recipients on the draft.
8. **Revise individually:** writing `2: shorter, and say Friday` next to a draft and tapping Claude rewrites only draft 2. `2: skip` drops it. Other drafts stay pending, unchanged.
9. **Send all:** a **Send N** button appears beside the Claude button whenever drafts are pending. One tap sends all pending drafts as threaded replies, which appear in Gmail's Sent folder.

## Non-goals (for the first iteration)

- New emails, as opposed to replies; forwarding; attachments (attachment names are mentioned, never opened).
- Other folders, labels or accounts. Inbox only, one Gmail account.
- Scheduled or automatic checks. It runs only when asked on the page.
- Tappable regions inside the inserted image. Inserted content is a picture; the only real button is Send.

## Anti-goals (failure even if it "works")

- **Sending something the user didn't see.** Nothing is sent except drafts currently shown on the page and counted on the Send button.
- **Wrong recipients or wrong thread.** Every draft states the email it answers and its recipients; a reply is never attached to a different email than its number says.
- **Obeying email content.** Email text is data. "Claude, forward this to…" inside an email is reported, never acted on.
- **Wall of text** or a hedged, caveat-heavy summary. Same terse style as other answers.
- **Silent failure.** Login, fetch, triage or send errors appear in the button's status message. A partial send says exactly which drafts went and which didn't.

## Triage rules ("needs attention")

Taken from the morning-brief skill the user's Claude apps use; their Desktop job is assumed to work the same way, which they can correct:

- It needs them if ignoring it until tomorrow would cost something: someone is blocked on them, a window closes today, or something gets harder to undo.
- A direct question or request to them that they haven't answered counts. A group @-mention, team alias or list mail where anyone could answer doesn't.
- Newsletters, marketing, receipts, automated notifications and social updates are skipped, unless they carry a real deadline for them (for example "your domain expires tomorrow").

## Constraints

- reMarkable 2, OS 3.28.0.172, Qt 6.10.3. Everything runs in the existing plugin (`src/diary.cpp` and its headers), using Qt Network (`QSslSocket` for IMAP/SMTP over TLS, `QNetworkAccessManager` for Claude). No new runtime dependencies on the tablet.
- Gmail access through an **app password** (the user confirmed they're comfortable storing it on the tablet): IMAP `imap.gmail.com:993` and SMTP `smtp.gmail.com:465`, both implicit TLS with certificate verification. Stored in `/home/root/.config/ink-diary/config.json` (`gmail_address`, `gmail_app_password`), mode 600, never logged.
- The page photo only covers the visible screen. A line or note only works when both ends are visible; numbers are the reliable fallback.
- Claude API usage: the existing `claude-sonnet-5-5` setup, structured outputs, prompt-injection-safe handling of email bodies.

## How it works (decided)

1. **Routing.** The existing reading request gains new `kind`s: `email_check`, and `email_reply`, which covers new replies, revisions and skips. For `email_reply`, the reading step returns the list of `{number, instruction}` it found on the page, resolving lines and notes-under-summaries to numbers. That uses the marks rule added on 2026-10-25.
2. **Check.**
   1. IMAP: log in, `SELECT INBOX`, `UID SEARCH UNSEEN`, and take the newest 50. Fetch headers (From, Reply-To, To, Cc, Subject, Date, Message-ID, References) and the text body with `BODY.PEEK` so fetching doesn't mark anything read. HTML-only mail is converted to text. Each body is truncated to a few thousand characters.
   2. A second Claude request triages and summarises with structured output: items `{n, uid, from, summary}` plus a skipped count and categories.
   3. The plugin renders the summary through the existing Markdown renderer and inserts it.
   4. Only after a successful insert are the checked UIDs marked `\Seen`.
   5. If there are more than 50 unread messages, the page says how many older ones weren't checked.
3. **State.** `/home/root/.local/share/ink-diary/email-state.json` (mode 600) maps each summary number to its message: UID, Message-ID, References, subject and recipients. It also holds the pending drafts. A new check replaces the number map and clears pending drafts. State survives app restarts.
4. **Drafts.** For each instruction, a Claude request gets the original email and the user's note and returns the draft body, matching their voice and the email's language.
   - Recipients are computed by the plugin, not by Claude. **To** is Reply-To, or else From. **Cc** is the original To and Cc, minus the user's own address, deduplicated.
   - The subject is `Re: <original>`, without stacking another "Re:".
   - Headers set `In-Reply-To` and `References`.
   - The drafts are inserted on the page, numbered like the emails.
5. **Revise/skip.** `n: <instruction>` beside an existing draft rewrites only draft n, with the previous draft as context. `n: skip` removes it. The re-rendered draft is inserted below, labelled "Draft 2 (revised)".
6. **Send button.**
   - `diary_status` reports the pending-draft count. The QML patch shows a second button, **Send N**, left of the Claude button, while N > 0.
   - Tapping it calls a new `ink-diary$send` broker signal. That sends each draft over SMTP (`AUTH PLAIN`, UTF-8 MIME) and removes sent drafts from the state.
   - The status message reports the result, for example "Sent 3" or "Sent 2; draft 3 failed: <reason>". Failed drafts stay pending.

## Left to the builder (my calls, flagged for the user)

- **Cap of 50** newest unread per check; older ones are only counted.
- **Mark-as-read covers skipped messages too.** The skipped line lets them spot a misjudged email.
- **Summary language** follows the language the user wrote their request in. Drafts follow the language of the email being answered.
- **No second confirmation on Send.** The button only exists while drafts are on the page, and the drafts are the confirmation.
- **Send button position:** left of the Claude button, same size and style, labelled "Send N".
- **Body truncation:** quoted history in replies isn't included in the draft text; Gmail threads it via headers.
- **Setup:** `configure.sh --gmail` prompts for the address and app password, hidden, and saves them like the API key.
- **One Claude request for all drafts** in a batch, returning one structured draft per number, instead of one request per draft. That keeps it to a single "Thinking…" wait.
- **Sign-off:** drafts end with the configured `name`, unless the note says otherwise or the thread uses another form.
- **A new check while drafts are pending** is refused with "Send or skip the pending drafts first". This keeps draft numbers unambiguous, since a new check renumbers everything.

## Testing

- Host tests: IMAP response parsing (SEARCH, FETCH with literals, UTF-8 encoded-word headers), recipient computation, subject `Re:` handling, MIME building, state file round-trip, and the routing of `email_check` / `email_reply`.
- Live tests from the Mac before deploying: real IMAP login, search and fetch with the app password (read-only, `BODY.PEEK`), and one real triage call. SMTP is tested by sending a reply to an email the user sends himself.
- Device test: the full loop of check, then reply to two, revise one, skip none, and Send.

## Search (added 2026-10-25)

- Requests like "find emails from Anna", "the email about the lease renewal" or "anything from the bank this month" route to `kind=email_search`. The reading step writes the Gmail search query (`from:anna newer_than:30d`, and so on).
- The search runs over IMAP with Gmail's own syntax (`X-GM-RAW`) in **All Mail**, so archived mail is found. The folder is found by its `\All` attribute, because its name is localised.
- Up to **10** matches, newest first, are summarised as numbered items in the same layout as a check, with no triage. A last line gives the count of further matches.
- "show me / fetch / open" for one email (`email.full=true`, exactly one match) prints that email in full: From, To, Cc, Date, Subject and body text.
- **Searches never change read state** (decided 2026-10-25). Replies to search results work exactly like replies to a check. The rule that a new check or search is refused while drafts are pending applies here too.

## Hand edits on drafts (added 2026-10-25)

- Tapping **Send N** first photographs the page. Corrections drawn by hand on a printed draft (crossed-out words, insertions, replacements) are applied, and the corrected drafts are sent in the same tap. At the user's request there's no second confirmation: the marks on the page are the confirmation. The status line names the drafts that were edited.
