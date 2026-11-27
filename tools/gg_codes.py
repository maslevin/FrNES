#!/usr/bin/env python3
"""gg_codes.py SPREADSHEET.xlsx OUT.txt.gz: the NES sheet (named NES, else the first) of a Game Genie code compilation (columns: game, #, codes,
effect) as the menus' code list: a gzipped text file, a line each "game<TAB>codes<TAB>effect", the codes joined by '+'
(up to three, one Game Genie's worth), a note about a game's codes as a line with no codes. Reads the .xlsx itself
(its XML), so no spreadsheet library is needed."""
import gzip, html, re, sys, zipfile

LETTERS = "APZLGITYEOXUKSVN"
CODE = "[%s]{6}(?:[%s]{2})?" % (LETTERS, LETTERS)


def sheet_rows(book, name):
    # (the sheet named 'name', else the workbook's first: tools/assets/Game_Genie_Database.xlsx has one, Sheet1)
    workbook = book.read("xl/workbook.xml").decode()
    rels = book.read("xl/_rels/workbook.xml.rels").decode()
    sheets = re.findall(r'<sheet [^>]*name="([^"]+)"[^>]*r:id="([^"]+)"', workbook)
    sheet_id = dict(sheets).get(name, sheets[0][1])
    target = dict(re.findall(r'Id="([^"]+)"[^>]*Target="([^"]+)"', rels))[sheet_id]
    strings = [html.unescape("".join(re.findall(r"<t[^>]*>(.*?)</t>", si, re.S)))
               for si in re.findall(r"<si>(.*?)</si>", book.read("xl/sharedStrings.xml").decode(), re.S)]
    xml = book.read("xl/" + target.lstrip("/").replace("xl/", "")).decode()
    for row in re.findall(r"<row [^>]*>(.*?)</row>", xml, re.S):
        cells = {}
        for column, attributes, inner in re.findall(r'<c r="([A-Z]+)\d+"([^>]*?)(?:/>|>(.*?)</c>)', row, re.S):
            value = re.search(r"<v>(.*?)</v>", inner or "")
            inline = re.search(r"<is>.*?<t[^>]*>(.*?)</t>", inner or "", re.S)
            if 't="s"' in attributes and value:
                cells[column] = strings[int(value.group(1))]
            elif inline:
                cells[column] = html.unescape(inline.group(1))
            elif value:
                cells[column] = value.group(1)
        yield cells


def clean(text):
    return " ".join(text.replace("\t", " ").split())


def main():
    book = zipfile.ZipFile(sys.argv[1])
    lines = []
    for cells in sheet_rows(book, "NES"):
        game, codes, effect = clean(cells.get("A", "")), clean(cells.get("C", "")), clean(cells.get("D", ""))
        if not game or game == "Game" or game.startswith("NES Official"):
            continue
        if re.fullmatch(r"%s(?: \+ %s){0,2}" % (CODE, CODE), codes):
            lines.append("%s\t%s\t%s" % (game, codes.replace(" + ", "+"), effect))
        elif codes:
            # (a note about the game's codes)
            lines.append("%s\t\t%s" % (game, codes))
    with gzip.open(sys.argv[2], "wt", encoding="latin-1", errors="replace") as out:
        out.write("\n".join(lines) + "\n")
    print("%d lines" % len(lines))


main()
