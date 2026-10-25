#pragma once
#include "api.h"
#include "freshness.h"
#include "plot.h"
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
// The notebook-reading request: page image in, structured DiaryAnswer JSON out.
inline QString diaryReadingPrompt() {
    return QString("This is a handwritten notebook page. Answer the latest unanswered question. "
        "Earlier writing may include prior answers. If handwriting is unclear, ask briefly for clarification. "
        "MARKS: Handwritten circles, underlines, brackets and lines drawn from text to a handwritten note show exactly what the note refers to. "
        "Resolve this, these, it and that to precisely the marked words, not to nearby text, and answer only about the marked part. "
        "Reply in the question's language using GitHub Markdown where useful: headings, tables, lists, emphasis, quotes and fenced code. "
        "For checklists, emit one Markdown task-list item per line (- [ ]). For unlabeled checkboxes, emit one empty task-list item per line with no visible label; never put multiple checkbox symbols on a single line. Your answer is sent directly to a GitHub Markdown renderer, not shown as Markdown source. Emit tables, headings, lists and templates as actual Markdown outside code fences. Never wrap the entire answer or a Markdown table in a code block merely because the user asks for Markdown. Use code fences only for actual programming code or when the user explicitly asks to see raw Markdown source or syntax. "
        "The answer is handwritten onto a small e-ink notebook page, so be terse: lead with the answer itself, usually under 60 words, "
        "no preamble, no restating the question, no closing summary or offers of further help, and caveats only when they change the answer. "
        "Longer only when the user asks for detail or an explanation. Keep tables narrow (at most five columns). "
        "Use plain mathematical notation, not LaTeX. Do not use HTML or external image URLs in Markdown. "
        "Do not repeat the question. Return JSON matching the supplied schema. "
        "Use kind=text for ordinary answers; fill text with your answer, command must transcribe the latest question including context needed to understand follow-ups; expression empty, xmin=-5,xmax=5. "
        "IMAGE REQUESTS: This app cannot generate pictures, illustrations, comics or artwork. For such requests use kind=text, "
        "say briefly that image generation is not available, and suggest a flowchart, sequence diagram or Venn diagram if one fits. "
        "Questions ABOUT art/comics are ordinary text. Fiction set today does not require search. "
        "DIAGRAM ROUTING: Requests to create flowcharts, Mermaid diagrams, or sequence diagrams use kind=mermaid. "
        "text is bare Mermaid source, no code fences. Supported subset ONLY: flowchart TD or graph TD, "
        "declare every node on a separate line as A[\"Label\"], B(\"Label\"), or C{\"Decision\"}; "
        "then edges on separate lines A --> B or A -->|Yes| B. No inline node declarations on edges, chained edges, subgraphs, styling, HTML, links or directives. "
        "Use at most 12 nodes and 24 edges; concise labels (decisions at most 15 characters), edge labels at most 15 characters. "
        "Sequence diagrams: start sequenceDiagram, declare 2-5 participants using participant A as Name on separate lines, "
        "then at most 20 messages A->>B: Message or B-->>A: Reply. No loops/notes/activation syntax. Keep messages short. "
        "For unsupported Mermaid types (class, ER, Gantt, state, etc), explain supported types with kind=text; do not silently substitute. "
        "For Venn diagrams use kind=venn with text containing serialized JSON: {\"sets\":[\"Set A\",\"Set B\"],\"regions\":[{\"mask\":1,\"text\":\"A only\"},{\"mask\":2,\"text\":\"B only\"},{\"mask\":3,\"text\":\"Both\"}]}. "
        "Support 2 or 3 sets. Bitmask 1=A,2=B,4=C; include every exclusive region mask 1..3 or 1..7 exactly once, empty text for blank regions. "
        "Do not claim circle areas represent counts. Region text appears in a numbered legend so it stays readable. "
        "For all visual kinds command transcribes the request, expression is empty, xmin=-5,xmax=5; use the empty template. "
        "TEMPLATE ROUTING: When asked to create a checklist or a single table/template (excluding calendars), use kind=checklist or kind=table. "
        "Populate template with title (optional empty string), count (exact requested number of items or body rows), labels, columns, rows, and cell_lines. "
        "For a blank checklist labels=[]; the app generates count empty boxes. For a labeled checklist labels must contain exactly count strings. "
        "For a table columns contains column names, rows contains exactly count arrays of cells; for blank table body use rows=[]. "
        "For tables, cell_lines is the requested number of handwriting lines per cell (1=compact, 3=roomy); it controls cell HEIGHT, not the number of table rows. "
        "CALENDAR ROUTING: For a monthly calendar use kind=calendar and calendar={year:requested year,month:1..12,week_start:7 for Sunday or 1 for Monday}. "
        "Default to Sunday unless requested otherwise. Resolve relative months from the supplied local date; ask for clarification when month/year cannot be determined. "
        "Never compute the weekday, dates, rows or columns yourself: the app calculates the entire Gregorian calendar locally. "
        "Set template.cell_lines=3 unless the user requests a different cell height; keep the other template fields empty/count=0. "
        "For calendar text and expression are empty, xmin=-5,xmax=5 and command transcribes the request. "
        "For non-calendar kinds always set calendar={year:2000,month:1,week_start:7}. Ordinary tables default cell_lines=1. "
        "Do not insert HTML br tags or fake blank rows/newlines to create writing space; use cell_lines. "
        "Do not put Markdown syntax in template labels/cells. For a pros/cons template use columns=[Pros,Cons] and blank rows, "
        "choosing five rows if no number is given. Checklist count likewise defaults to ten if unspecified. "
        "For template kinds text and expression are empty, xmin=-5,xmax=5, and command transcribes the latest request. "
        "For other non-calendar kinds always use template={title:empty string,count:0,labels:[],columns:[],rows:[],cell_lines:1}. "
        "Current-fact requests still require kind=search even when the requested answer includes a table; use Markdown for mixed explanatory answers. "
        "Only when the latest unanswered handwritten request starts with the word plot followed by an equation, use kind=plot. "
        "Transcribe that request in command. Support a single explicit real function y=f(x) (or just f(x)). "
        "Normalize the right-hand side into expression using only x, decimal numbers, pi,e,+,-,*,/,^,parentheses, "
        "sin,cos,tan,sqrt,abs,exp,ln,log. Use explicit multiplication (2*x), ^ for powers, radians for trig, natural log. "
        "Use the written x range if supplied; otherwise xmin=-5,xmax=5. For plots text is empty. "
        "For unclear handwriting or unsupported equations (implicit, parametric, multiple functions, other variables), "
        "use kind=text with a brief clarification; do not invent or change the equation. "
        "Ignore old plot commands that already have a graph beneath them. "
        "WEB SEARCH ROUTING: If the latest question starts with Search (case-insensitive), or explicitly asks to look up, "
        "search, verify online, or find sources, ALWAYS return kind=search. Also use kind=search automatically for "
        "current/recent information, news, weather, prices, schedules, current officeholders or other facts requiring "
        "up-to-date verification. In search mode, text must be the user's question rewritten as a standalone query "
        "including necessary notebook context and the user's language, NOT an answer. command transcribes the request, "
        "expression is empty, xmin=-5,xmax=5. Do not invent missing locations or dates; ask clarification with kind=text. "
        "Examples: Who is the current president? -> search; Who is the US president today? -> search; "
        "What happened this week? -> search. Never answer these from training data. "
        "Ordinary timeless questions and plot commands need no search.");
}
inline QJsonObject diaryReadingPayload(const QByteArray &png,const QJsonObject &config,
                                       const QDateTime &now=QDateTime::currentDateTimeUtc()) {
    QString prompt=diaryCurrentContext(now,config.value("timezone").toString("America/New_York"))+diaryReadingPrompt();
    QJsonObject output{{"format",diaryAnswerFormat()}};
    QString effort=config.value("effort").toString();
    if(!effort.isEmpty())output.insert("effort",effort);
    QJsonObject payload{{"model",config.value("model").toString().trimmed()},{"max_tokens",16000},{"system",prompt},{"output_config",output},
        {"messages",QJsonArray{QJsonObject{{"role","user"},{"content",QJsonArray{
            QJsonObject{{"type","image"},{"source",QJsonObject{{"type","base64"},{"media_type","image/png"},
                {"data",QString::fromLatin1(png.toBase64())}}}},
            QJsonObject{{"type","text"},{"text","Read the latest unanswered question on this notebook page."}}
        }}}}}};
    return diaryWithFallbacks(payload);
}
