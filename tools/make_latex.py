"""
make_latex.py -- the project documentation as LaTeX: docs/latex/VOID_RUNNER_documentation.tex
(+ figures/), compiled to docs/latex/VOID_RUNNER_documentation.pdf with XeLaTeX.

Same content as tools/make_doc.py (the Word version): this script runs make_doc's section
functions with a LaTeX writer instead of the Word writer, so both versions always agree.
For Overleaf: upload the docs/latex folder and set the compiler to XeLaTeX.

Usage (from the project folder, after `python tests/host/run_tests.py` built build/sim.exe):
    python tools/make_latex.py            write the .tex and compile it
    python tools/make_latex.py --no-pdf   only write the .tex
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import make_doc as md                                        # noqa: E402

OUT_DIR = ROOT / 'docs' / 'latex'
FIG_DIR = OUT_DIR / 'figures'
TEX = OUT_DIR / 'VOID_RUNNER_documentation.tex'
TEXT_WIDTH_CM = 17.0


def esc(text):
    """Escapes LaTeX's special characters in plain text."""
    repl = {'\\': r'\textbackslash{}', '&': r'\&', '%': r'\%', '$': r'\$', '#': r'\#',
            '_': r'\_', '{': r'\{', '}': r'\}', '~': r'\textasciitilde{}',
            '^': r'\textasciicircum{}'}
    return ''.join(repl.get(c, c) for c in str(text))


def rich(text):
    """Plain text with **bold** and `code` parts, as LaTeX."""
    out = []
    for part in re.split(r'(\*\*.+?\*\*|`.+?`)', str(text)):
        if part.startswith('**'):
            out.append(r'\textbf{%s}' % esc(part[2:-2]))
        elif part.startswith('`'):
            out.append(r'\texttt{%s}' % esc(part[1:-1]))
        else:
            out.append(esc(part))
    return ''.join(out)


class TexDoc:
    """Writes the same calls as make_doc.Doc (h, p, rich, table, gallery ...) as LaTeX."""

    def __init__(self):
        self.out = []
        self.figs = {}

    def w(self, text):
        self.out.append(text)

    def fig(self, path):
        """Copies a picture into figures/ and returns its path relative to the .tex."""
        path = Path(path)
        if path not in self.figs:
            name = path.name
            if name in {Path(p).name for p in self.figs.values()}:
                name = '%d_%s' % (len(self.figs), name)
            shutil.copyfile(path, FIG_DIR / name)
            self.figs[path] = 'figures/' + name
        return self.figs[path]

    def h(self, text, level):
        cmd = {1: 'section', 2: 'subsection', 3: 'subsubsection'}[level]
        self.w('\\%s*{%s}\n\\phantomsection\\addcontentsline{toc}{%s}{%s}\n'
               % (cmd, esc(text), cmd, esc(text)))

    def p(self, text='', **_):
        self.w((esc(text) if text else '') + '\n\n')

    def rich(self, text):
        self.w(rich(text) + '\n\n')

    def bullets(self, items):
        self.w('\\begin{itemize}\n' + ''.join('  \\item %s\n' % rich(i) for i in items)
               + '\\end{itemize}\n\n')

    def picture(self, path, width_cm, caption=None):
        self.w('\\begin{figure}[H]\\centering\n\\includegraphics[width=%.2fcm]{%s}\n'
               % (min(width_cm, TEXT_WIDTH_CM), self.fig(path)))
        if caption:
            self.w('\\caption{%s}\n' % rich(caption))
        self.w('\\end{figure}\n\n')

    def gallery(self, items, per_row, width_cm, caption=None):
        self.w('\\begin{figure}[H]\\centering\n')
        for i, (path, label) in enumerate(items):
            if i and i % per_row == 0:
                self.w('\\\\[6pt]\n')
            self.w('\\begin{minipage}[t]{%.2fcm}\\centering\n'
                   '\\includegraphics[width=\\linewidth]{%s}\\\\[-2pt]\n'
                   '{\\scriptsize\\color{grey}%s}\n\\end{minipage}\\hspace{4pt}%%\n'
                   % (width_cm, self.fig(path), esc(label)))
        if caption:
            self.w('\\caption{%s}\n' % rich(caption))
        self.w('\\end{figure}\n\n')

    def table(self, header, rows, widths=None, caption=None, size=9.5):
        n = len(header)
        widths = widths or [TEXT_WIDTH_CM / n] * n
        scale = (TEXT_WIDTH_CM - 0.25 * n) / sum(widths)      # room for the cell padding
        cols = '|'.join('>{\\raggedright\\arraybackslash}p{%.2fcm}' % (w * scale) for w in widths)
        fs = '\\small' if size >= 9.5 else ('\\footnotesize' if size >= 8.5 else '\\scriptsize')
        head = ' & '.join('\\textbf{\\color{white}%s}' % esc(h) for h in header)
        body = ''.join('%s%s\\\\\\hline\n' % ('\\rowcolor{rowshade}' if r % 2 else '',
                                                ' & '.join(rich(c) for c in row))
                       for r, row in enumerate(rows))
        if len(rows) <= 12:               # short: kept in one piece (a minipage never breaks)
            self.w('\\par\\noindent\\begin{minipage}{\\linewidth}\\centering%s\n'
                   '\\begin{tabular}{|%s|}\n\\hline\\rowcolor{accent}%s\\\\\\hline\n%s'
                   '\\end{tabular}\n' % (fs, cols, head, body))
            if caption:
                self.w('\\captionof{table}{%s}\n' % rich(caption))
            self.w('\\end{minipage}\\par\\medskip\n\n')
            return
        self.w('{%s\\begin{longtable}{|%s|}\n\\hline\\rowcolor{accent}%s\\\\\\hline\\endhead\n%s'
               % (fs, cols, head, body))
        if caption:
            self.w('\\caption{%s}\\\\\n' % rich(caption))
        self.w('\\end{longtable}}\n\n')

    def code(self, text, lang):
        language = {'c': 'Arduino', 'py': 'Python'}.get(lang, '')
        body = text.replace('\t', '    ').rstrip('\n')
        assert '\\end{lstlisting}' not in body
        self.w('\\begin{lstlisting}[language=%s]\n%s\n\\end{lstlisting}\n\n' % (language or '{}', body))

    def page_break(self):
        self.w('\\clearpage\n')


PREAMBLE = r"""\documentclass[11pt,a4paper]{article}
% VOID RUNNER -- project documentation. GENERATED by tools/make_latex.py; compile with XeLaTeX.
\usepackage[margin=2cm]{geometry}
\usepackage{fontspec}
\IfFontExistsTF{Arial}{\setmainfont{Arial}}{}
\IfFontExistsTF{Consolas}{\setmonofont{Consolas}}{}
\usepackage[table]{xcolor}
\usepackage{graphicx,float,longtable,array,listings,enumitem}
\usepackage[font=small,labelfont=bf,textfont=it]{caption}
\usepackage{titlesec}
\usepackage[hidelinks,bookmarksnumbered=false]{hyperref}

\definecolor{accent}{HTML}{1F3A5F}
\definecolor{rowshade}{HTML}{EEF2F7}
\definecolor{grey}{HTML}{606060}
\definecolor{codebg}{HTML}{F6F8FA}
\definecolor{codecomment}{HTML}{2E7D32}
\definecolor{codekeyword}{HTML}{1F3A5F}
\definecolor{codestring}{HTML}{B23A1D}
\definecolor{codepre}{HTML}{8E24AA}

\titleformat*{\section}{\LARGE\bfseries\color{accent}}
\titleformat*{\subsection}{\Large\bfseries\color{accent}}
\titleformat*{\subsubsection}{\large\bfseries\color{accent}}
\setlength{\parindent}{0pt}
\setlength{\parskip}{5pt}
\setlist{itemsep=2pt}
\renewcommand{\arraystretch}{1.25}
\setlength{\LTpre}{4pt}
\setlength{\LTpost}{8pt}

\lstset{
  basicstyle=\ttfamily\fontsize{7.2}{8.6}\selectfont,
  backgroundcolor=\color{codebg},
  numbers=left, numberstyle=\tiny\color{grey}, numbersep=6pt, xleftmargin=16pt,
  breaklines=true, breakatwhitespace=false, columns=fixed, keepspaces=true,
  showstringspaces=false, tabsize=4, frame=none, framesep=0pt,
  commentstyle=\color{codecomment}, keywordstyle=\color{codekeyword}\bfseries,
  stringstyle=\color{codestring},
  extendedchars=true,
  literate={×}{{$\times$}}1 {–}{{--}}1 {·}{{$\cdot$}}1 {÷}{{$\div$}}1 {…}{{...}}1 {→}{{$\to$}}1,
}
\lstdefinelanguage{Arduino}[ISO]{C++}{morekeywords={uint8_t,int8_t,uint16_t,int16_t,uint32_t,int32_t,
  bool,PROGMEM,ISR}, moredirectives={pragma}}

\begin{document}
"""


def cover(doc):
    c = md.COVER
    rows = [('Student', c['student']), ('Student ID', c['student_id']), ('Teacher', c['teacher']),
            ('Course', c['course']), ('Semester', c['semester']), ('Date', c['date']),
            ('Platform', 'Arduino Mega 2560, simulated in Wokwi'), ('Source code', c['repo'])]
    table = '\n'.join('\\textbf{\\color{accent}%s} & %s \\\\' % (esc(k), esc(v)) for k, v in rows)
    doc.w(r"""\begin{titlepage}\centering
\vspace*{2cm}
{\Large\bfseries %s\par}\vspace{4pt}
{\large %s\par}\vspace{1cm}
{\large %s\par}\vspace{2pt}
{\color{grey}%s\par}\vspace{2.2cm}
{\fontsize{40}{46}\selectfont\bfseries\color{accent} VOID RUNNER\par}\vspace{8pt}
{\Large\itshape A vertical space shooter on an Arduino Mega\par}\vspace{6pt}
{\color{grey}432 single LEDs $\cdot$ a 9-button keypad $\cdot$ 4 buzzers $\cdot$ no modules, no libraries\par}
\vspace{2.5cm}
{\large\begin{tabular}{@{}l@{\hspace{1.5cm}}l@{}}
%s
\end{tabular}\par}
\vfill
\end{titlepage}
""" % (esc(c['university']), esc(c['faculty']), esc(c['course']), esc(c['project']), table))


def main():
    pics = md.make_media()
    if FIG_DIR.exists():
        shutil.rmtree(FIG_DIR)
    FIG_DIR.mkdir(parents=True)
    doc = TexDoc()
    doc.w(PREAMBLE)
    cover(doc)
    md.hardware_page(doc, pics)
    md.performance(doc, pics)
    md.code_part(doc, pics)
    doc.w('\\end{document}\n')
    TEX.write_text(''.join(doc.out), encoding='utf-8', newline='\n')
    print('wrote', TEX, '(%d figures)' % len(doc.figs))
    if '--no-pdf' in sys.argv:
        return
    for _ in range(2):                                      # twice: figure and table numbers
        r = subprocess.run(['xelatex', '-interaction=nonstopmode', '-halt-on-error', TEX.name],
                           cwd=OUT_DIR, capture_output=True, text=True, errors='replace')
        if r.returncode:
            print(r.stdout[-3000:])
            sys.exit('xelatex failed')
    for ext in ('.aux', '.log', '.out', '.toc'):
        (OUT_DIR / (TEX.stem + ext)).unlink(missing_ok=True)
    print('wrote', TEX.with_suffix('.pdf'))


if __name__ == '__main__':
    main()
