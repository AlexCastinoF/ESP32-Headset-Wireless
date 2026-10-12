# -*- coding: utf-8 -*-
"""Gera o manual de engenharia, revisão B, do headset sem fio."""

from reportlab.lib.colors import HexColor, white
from reportlab.lib.enums import TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, Preformatted, KeepTogether, CondPageBreak, HRFlowable,
)

pdfmetrics.registerFont(TTFont("Segoe", r"C:\Windows\Fonts\segoeui.ttf"))
pdfmetrics.registerFont(TTFont("Segoe-Bold", r"C:\Windows\Fonts\segoeuib.ttf"))
pdfmetrics.registerFont(TTFont("Segoe-Italic", r"C:\Windows\Fonts\segoeuii.ttf"))
pdfmetrics.registerFont(TTFont("Segoe-BoldItalic", r"C:\Windows\Fonts\segoeuiz.ttf"))
pdfmetrics.registerFont(TTFont("Consolas", r"C:\Windows\Fonts\consola.ttf"))
pdfmetrics.registerFont(TTFont("Consolas-Bold", r"C:\Windows\Fonts\consolab.ttf"))
pdfmetrics.registerFontFamily(
    "Segoe",
    normal="Segoe",
    bold="Segoe-Bold",
    italic="Segoe-Italic",
    boldItalic="Segoe-BoldItalic",
)

INK = HexColor("#1C2834")
MUTED = HexColor("#5C6B78")
TEAL = HexColor("#0E7C72")
TEAL_DARK = HexColor("#14232E")
LINE = HexColor("#D5DDE4")
ZEBRA = HexColor("#F4F7F8")
PAPER = HexColor("#F3F7F7")
AMBER_BG = HexColor("#FBF6EC")
AMBER = HexColor("#8A5A10")
RED_BG = HexColor("#F8EEEE")
RED = HexColor("#8C3030")

W = 511  # largura útil, pontos

OUT = r"h:\Projeto HeadSet Wireless\Manual de Engenharia - Headset Wireless ESP32 - Revisao B.pdf"


def S(name, **kw):
    defaults = dict(fontName="Segoe", textColor=INK, leading=13)
    defaults.update(kw)
    return ParagraphStyle(name, **defaults)


styles = {
    "h1": S("h1", fontName="Segoe-Bold", fontSize=13.5, leading=17, textColor=TEAL_DARK,
            spaceBefore=11, spaceAfter=4, keepWithNext=True),
    "h2": S("h2", fontName="Segoe-Bold", fontSize=11, leading=14, textColor=TEAL,
            spaceBefore=8, spaceAfter=3, keepWithNext=True),
    "body": S("body", fontSize=9.5, leading=13.2, spaceAfter=6),
    "small": S("small", fontSize=8.5, leading=11.4, textColor=MUTED, spaceAfter=6),
    "td": S("td", fontSize=8, leading=10.4, spaceAfter=0),
    "th": S("th", fontName="Segoe-Bold", fontSize=8, leading=10.2, textColor=white, spaceAfter=0),
    "kicker": S("kicker", fontName="Segoe-Bold", fontSize=7, leading=9, textColor=TEAL, spaceAfter=0),
    "tilev": S("tilev", fontName="Segoe-Bold", fontSize=10.5, leading=13, textColor=TEAL_DARK, spaceAfter=1),
    "tiled": S("tiled", fontSize=7.5, leading=9.6, textColor=MUTED, spaceAfter=0),
    "cover_kicker": S("cover_kicker", fontName="Segoe-Bold", fontSize=8.5, leading=11,
                      textColor=TEAL, spaceAfter=2),
    "cover_title": S("cover_title", fontName="Segoe-Bold", fontSize=24, leading=28,
                     textColor=TEAL_DARK, spaceAfter=2),
    "cover_sub": S("cover_sub", fontSize=11, leading=15, textColor=INK, spaceAfter=4),
    "caption": S("caption", fontName="Segoe-Italic", fontSize=8, leading=10.5,
                 textColor=MUTED, spaceBefore=2, spaceAfter=8),
    "call_title": S("call_title", fontName="Segoe-Bold", fontSize=8.5, leading=11, spaceAfter=1),
    "call_body": S("call_body", fontSize=8.5, leading=11.4, spaceAfter=0),
    "step": S("step", fontSize=8.5, leading=11.2, spaceAfter=1),
    "footer": S("footer", fontSize=8, leading=10, textColor=MUTED),
    "path_h": S("path_h", fontName="Segoe-Bold", fontSize=8.5, leading=11, textColor=white, spaceAfter=0),
    "path_b": S("path_b", fontSize=8, leading=10.8, textColor=INK, spaceAfter=0),
}


def P(text, style="body"):
    return Paragraph(text, styles[style])


def code_block(text, size=7.4, lead=9.5):
    pre_style = ParagraphStyle(
        "pre",
        fontName="Consolas",
        fontSize=size,
        leading=lead,
        textColor=INK,
    )
    pre = Preformatted(text.rstrip("\n"), pre_style, maxLineLength=110)
    table = Table([[pre]], colWidths=[W])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), HexColor("#F4F7F8")),
        ("BOX", (0, 0), (-1, -1), 0.4, LINE),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    table.splitByRow = 0
    return table


def callout(title, body, kind="note"):
    palette = {
        "note": (PAPER, TEAL, TEAL_DARK),
        "warn": (AMBER_BG, AMBER, AMBER),
        "stop": (RED_BG, RED, RED),
    }
    bg, accent, title_color = palette[kind]
    title_style = ParagraphStyle(
        f"call_title_{kind}_{title[:12]}",
        parent=styles["call_title"],
        textColor=title_color,
    )
    title_p = Paragraph(title, title_style)
    body_p = Paragraph(body, styles["call_body"])
    inner = Table([[title_p], [body_p]], colWidths=[W - 8])
    inner.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LINEBEFORE", (0, 0), (0, -1), 3, accent),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (0, 0), 6),
        ("BOTTOMPADDING", (0, 0), (0, 0), 1),
        ("TOPPADDING", (0, 1), (0, 1), 1),
        ("BOTTOMPADDING", (0, 1), (0, 1), 7),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
    ]))
    inner.splitByRow = 0
    return inner


def table(headers, rows, widths, split=True):
    assert abs(sum(widths) - W) < 2, sum(widths)
    head = [Paragraph(h, styles["th"]) for h in headers]
    data = [head]
    for row in rows:
        data.append([Paragraph(c, styles["td"]) for c in row])
    tbl = Table(data, colWidths=widths, repeatRows=1)
    cmds = [
        ("BACKGROUND", (0, 0), (-1, 0), TEAL_DARK),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("GRID", (0, 0), (-1, -1), 0.3, LINE),
    ]
    for i in range(1, len(data)):
        if i % 2 == 0:
            cmds.append(("BACKGROUND", (0, i), (-1, i), ZEBRA))
    tbl.setStyle(TableStyle(cmds))
    tbl.splitByRow = 1 if split else 0
    return tbl


def tile(kicker, value, detail, width):
    inner_w = width - 8
    bits = [
        [Paragraph(kicker.upper(), styles["kicker"])],
        [Paragraph(value, styles["tilev"])],
        [Paragraph(detail, styles["tiled"])],
    ]
    inner = Table(bits, colWidths=[inner_w])
    inner.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), PAPER),
        ("LINEABOVE", (0, 0), (-1, 0), 2.5, TEAL),
        ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (0, 0), 6),
        ("BOTTOMPADDING", (0, 0), (0, 0), 0),
        ("TOPPADDING", (0, 1), (0, 1), 2),
        ("BOTTOMPADDING", (0, 1), (0, 1), 1),
        ("TOPPADDING", (0, 2), (0, 2), 1),
        ("BOTTOMPADDING", (0, 2), (0, 2), 7),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
    ]))
    return inner


def spec_grid():
    tw = W / 3.0
    items = [
        ("Ouvido", "48 kHz estéreo", "ADPCM 4 bit no ar. Um pacote a cada 5 ms."),
        ("Voz", "16 kHz mono", "PCM 16 bit, sem compressão. Parte de 48 kHz."),
        ("Latência", "25 a 40 ms", "Do PC até o ouvido. Ida e volta: 55 a 85 ms."),
        ("Rádio", "ESP-NOW", "Canal 6 fixo. Sem roteador. Sem modo long range."),
        ("Energia", "1S · 1600 mAh", "Buck-boost para 3,3 V. Bateria não entra no VIN."),
        ("PC", "USB Audio Class", "S3 com USB nativo. Sem driver no Windows."),
    ]
    row1 = [tile(*items[i], tw) for i in range(3)]
    row2 = [tile(*items[i], tw) for i in range(3, 6)]
    grid = Table([row1, row2], colWidths=[tw, tw, tw])
    grid.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 3),
        ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("BACKGROUND", (0, 0), (-1, -1), white),
    ]))
    return grid


def path_card(title, lines, header_color):
    body = "<br/>".join(lines)
    head = Paragraph(title, styles["path_h"])
    text = Paragraph(body, styles["path_b"])
    card = Table([[head], [text]], colWidths=[(W - 10) / 2])
    card.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), header_color),
        ("BACKGROUND", (0, 1), (-1, 1), PAPER),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7),
        ("TOPPADDING", (0, 0), (0, 0), 5),
        ("BOTTOMPADDING", (0, 0), (0, 0), 5),
        ("TOPPADDING", (0, 1), (0, 1), 6),
        ("BOTTOMPADDING", (0, 1), (0, 1), 7),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("BOX", (0, 0), (-1, -1), 0.3, LINE),
    ]))
    return card


def paths():
    left = path_card("DESCIDA · JOGO E MÚSICA", [
        "1. O PC entrega 48 kHz, 16 bit, estéreo pelo USB.",
        "2. O ESP32-S3 codifica IMA ADPCM de 4 bit.",
        "3. Um pacote ESP-NOW de 250 bytes sai a cada 5 ms.",
        "4. O ESP32-C3 decodifica e devolve PCM 48 kHz.",
        "5. PCM5102A, potenciómetro, TPA6132, drivers de 32 Ω.",
    ], TEAL_DARK)
    right = path_card("SUBIDA · VOZ", [
        "1. O INMP441 é lido a 48 kHz no mesmo I2S do DAC.",
        "2. A cada 3 amostras, o C3 guarda a média: 16 kHz.",
        "3. 80 amostras PCM seguem no pacote de resposta.",
        "4. O S3 interpola 16 kHz para 48 kHz.",
        "5. O Windows vê um microfone USB de 48 kHz, mono.",
    ], TEAL)
    wrap = Table([[left, right]], colWidths=[W / 2, W / 2])
    wrap.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (0, 0), 0),
        ("RIGHTPADDING", (0, 0), (0, 0), 5),
        ("LEFTPADDING", (1, 0), (1, 0), 5),
        ("RIGHTPADDING", (1, 0), (1, 0), 0),
        ("TOPPADDING", (0, 0), (-1, -1), 0),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
    ]))
    return wrap


def header_footer(canvas, doc):
    canvas.saveState()
    width, height = A4
    canvas.setFillColor(TEAL_DARK)
    canvas.rect(0, height - 26, width, 26, fill=1, stroke=0)
    canvas.setFillColor(TEAL)
    canvas.rect(0, height - 29, width, 3, fill=1, stroke=0)
    canvas.setFillColor(white)
    canvas.setFont("Segoe", 8)
    canvas.drawString(16 * mm, height - 16, "HEADSET WIRELESS   ·   ESP32-C3 NO FONE   ·   ESP32-S3 NO DONGLE")
    canvas.setFont("Segoe-Bold", 8)
    canvas.drawRightString(width - 16 * mm, height - 16, "REVISÃO B")
    canvas.setStrokeColor(LINE)
    canvas.line(16 * mm, 13 * mm, width - 16 * mm, 13 * mm)
    canvas.setFillColor(MUTED)
    canvas.setFont("Segoe", 8)
    canvas.drawString(16 * mm, 8 * mm, "Manual de engenharia   ·   10 de outubro de 2026")
    canvas.setFont("Segoe-Bold", 8)
    canvas.drawRightString(width - 16 * mm, 8 * mm, str(doc.page))
    canvas.restoreState()


def build():
    story = []

    story.append(P("MANUAL DE ENGENHARIA", "cover_kicker"))
    story.append(P("Headset sem fio", "cover_title"))
    story.append(P(
        "Base Razer BlackShark V2 X. O fone fica com um ESP32-C3 SuperMini. "
        "O dongle USB é um ESP32-S3 com USB nativo. Os dois falam ESP-NOW em 2,4 GHz, sem roteador.",
        "cover_sub",
    ))
    story.append(HRFlowable(width="100%", thickness=1, color=LINE, spaceBefore=1, spaceAfter=8))
    story.append(spec_grid())
    story.append(Spacer(1, 8))
    story.append(P(
        "Esta revisão substitui o mapa de fios e o formato de pacote do manual anterior. "
        "A mecânica continua a mesma: drivers originais de 50 mm e 32 Ω, duas LiPo de 800 mAh, "
        "quatro fios no arco e a roda de volume da concha esquerda. O que muda é o que torna o áudio possível: "
        "o rádio passa a carregar estéreo de verdade, o potenciómetro entra antes do amplificador, "
        "e a bateria alimenta um buck-boost de 3,3 V em vez do pino VIN."
    ))
    story.append(paths())
    story.append(P(
        "Os dois sentidos usam o mesmo relógio I2S de 48 kHz. O rádio do ESP32 é half-duplex: "
        "a descida e a subida se revezam dentro de cada quadro de 5 ms.",
        "caption",
    ))

    story.append(P("1. O que mudou", "h1"))
    story.append(table(
        ["Ponto", "Manual anterior", "Revisão B"],
        [
            ["Áudio no ar", "240 bytes de PCM, 16 kHz, um sentido só. Não cabia estéreo nem o microfone ao mesmo tempo.",
             "Descida: 48 kHz estéreo em ADPCM, um pacote. Subida: 16 kHz mono em PCM, no pacote de resposta."],
            ["Latência", "Meta abaixo de 10 ms. O próprio quadro já durava 7,5 ms, antes do USB e do rádio.",
             "Meta de 25 a 40 ms até o ouvido. O quadro é de 5 ms e o buffer de espera guarda dois quadros."],
            ["Volume", "Potenciómetro depois do TPA6132, em série com o driver de 32 Ω.",
             "Divisor fixo, potenciómetro e só então o TPA6132. O driver liga direto na saída do amplificador."],
            ["Alimentação", "LiPo no VIN do SuperMini. O regulador da placa não segura 3,3 V com a bateria em 3,7 V.",
             "Buck-boost de 3,3 V no pino 3V3. O VIN não é usado. USB do C3 só na gravação, com a chave desligada."],
            ["Pacote perdido", "Buffer sem número de sequência.",
             "Campo seq de 16 bit. Um buraco repete o quadro anterior 3 dB mais baixo. O ADPCM não perde o passo."],
            ["Mudo e bateria", "Bytes previstos, sem GPIO do mudo e sem capacitor no ADC.",
             "Mudo no GPIO6. Divisor 100 kΩ + 100 kΩ, 100 nF, atenuação de 12 dB e calibração de fábrica."],
        ],
        [78, 216, 217],
    ))

    story.append(P("2. A conta do áudio", "h1"))
    story.append(P(
        "O ESP-NOW entrega no máximo 250 bytes por pacote. Um quadro de 5 ms a 48 kHz estéreo e 16 bit "
        "ocuparia 960 bytes, quatro pacotes, sem folga para a voz nem para uma retransmissão. "
        "Comprimir cada amostra de 16 bit para 4 bit fecha a descida em exatamente 240 bytes de áudio."
    ))
    story.append(table(
        ["Trecho", "Formato", "Bytes em 5 ms", "Pacotes"],
        [
            ["Descida, jogo", "48 kHz, estéreo, IMA ADPCM 4 bit", "240 de áudio + 10 de cabeçalho = 250", "1"],
            ["Subida, voz", "16 kHz, mono, PCM 16 bit", "160 de áudio + 6 de cabeçalho = 166", "1"],
            ["Os dois", "Revezamento no mesmo canal", "416 bytes úteis", "2, dentro de 5 ms"],
        ],
        [100, 195, 166, 50],
        split=False,
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "240 amostras por canal a 48 kHz são 5,00 ms. 240 também divide por 3, então a decimação "
        "48 kHz para 16 kHz fecha o quadro em 80 amostras, sem sobra. Por isso o quadro é de 5 ms "
        "e não de 7,5 ms: o número serve ao mesmo tempo ao pacote de 250 bytes e ao microfone."
    ))
    story.append(table(
        ["Trecho da latência", "Tempo", "De onde vem"],
        [
            ["Encher o quadro", "5 ms", "É preciso ter as amostras antes de transmitir."],
            ["Ar", "0,5 a 1,5 ms", "Um pacote curto em 802.11n, sem modo long range."],
            ["Buffer de espera", "10 ms", "Dois quadros. Absorve um atraso sem estourar o áudio."],
            ["USB no Windows", "5 a 10 ms", "Quadro isócrono e motor de áudio do sistema."],
            ["DMA do I2S", "2 a 5 ms", "Dois buffers de 240 quadros estéreo."],
            ["Total até o ouvido", "25 a 40 ms", "Some os trechos da descida. Não inclui a volta do microfone."],
        ],
        [120, 78, 313],
    ))
    story.append(Spacer(1, 6))
    story.append(callout(
        "O que este rádio não promete",
        "Abaixo de 10 ms de ponta a ponta não cabe: só o quadro já gasta 5 ms e o buffer de espera, outros 10. "
        "Bluetooth A2DP no ESP32 costuma passar de 150 ms, então 25 a 40 ms continua curto para jogo. "
        "A qualidade da descida fica boa para voz e para o jogo. Música densa fica um degrau atrás de um codec SBC bem afinado. "
        "Se no futuro o C3 sobrar processamento, SBC é o upgrade. Não é o caminho desta montagem.",
        "note",
    ))

    story.append(P("3. Protocolo ESP-NOW", "h1"))
    story.append(P(
        "O dongle é o mestre do tempo. A cada 5 ms ele manda um pacote de descida. "
        "O fone responde com a voz assim que o pacote chega. Os dois ficam em modo estação, "
        "no canal 6, sem associar a nenhum roteador. Power save desligado. "
        "Criptografia desligada nesta revisão: o FCS do Wi-Fi já protege o pacote contra erro de bit; "
        "não esconde o áudio de quem estiver ao lado."
    ))
    story.append(P("Pacote de descida, dongle para o fone. 250 bytes, limite exato do ESP-NOW.", "h2"))
    story.append(code_block(
        "typedef struct __attribute__((packed)) {\n"
        "    uint16_t seq;          /* +1 a cada quadro de 5 ms */\n"
        "    uint8_t  type;         /* 1 = descida */\n"
        "    uint8_t  flags;        /* reservado, gravar 0 */\n"
        "    int16_t  pred_l;       /* preditor ADPCM antes da 1a amostra */\n"
        "    int16_t  pred_r;\n"
        "    uint8_t  index_l;      /* 0 a 88, tabela step IMA */\n"
        "    uint8_t  index_r;\n"
        "    uint8_t  audio[240];   /* 480 nibbles: L, R, L, R... */\n"
        "} PacoteDown;               /* 2+1+1+2+2+1+1+240 = 250 */"
    ))
    story.append(Spacer(1, 6))
    story.append(P("Pacote de subida, fone para o dongle. 166 bytes.", "h2"))
    story.append(code_block(
        "typedef struct __attribute__((packed)) {\n"
        "    uint16_t seq;          /* mesmo numero do quadro respondido */\n"
        "    uint8_t  type;         /* 2 = subida */\n"
        "    uint8_t  flags;        /* bit 0 = 1 quando o mudo esta fechado */\n"
        "    uint8_t  bateria;      /* 0 a 100, lido uma vez por segundo */\n"
        "    uint8_t  reservado;    /* gravar 0 */\n"
        "    int16_t  mic[80];      /* PCM 16 bit, 16 kHz, little-endian */\n"
        "} PacoteUp;                 /* 2+1+1+1+1+160 = 166 */"
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "No áudio da descida, o nibble de baixo de cada byte é a amostra da esquerda e o nibble de cima é a da direita. "
        "O byte seguinte é o par seguinte. pred e index são o estado <b>antes</b> da primeira amostra daquele quadro: "
        "o codificador grava o estado, depois codifica; o decodificador carrega o estado e depois decodifica. "
        "Um pacote perdido não estraga o quadro seguinte."
    ))
    story.append(P(
        "O callback do ESP-NOW só copia o pacote para uma fila. Quem responde, decodifica e escreve no I2S é uma task. "
        "Não chamar envio nem printf dentro do callback. Se o pacote de descida não chegar em 6 ms, "
        "o fone toca de novo o PCM anterior com o nível em cerca de 0,7 (−3 dB), e ainda assim manda a subida "
        "no timer local de 5 ms. Um buraco de mais de três sequências esvazia o buffer e recomeça."
    ))

    story.append(P("Contrato do IMA ADPCM", "h2"))
    story.append(P(
        "Os dois lados usam o IMA ADPCM clássico, também chamado DVI. Índice limitado a 0–88. "
        "Amostra limitada ao int16. Ajuste de índice, pelo nibble nos três bits de baixo: "
        "−1, −1, −1, −1, 2, 4, 6, 8. Para cada nibble o decodificador faz o seguinte."
    ))
    story.append(code_block(
        "step  = step_table[index]\n"
        "delta = step >> 3\n"
        "if (nibble & 4) delta += step\n"
        "if (nibble & 2) delta += step >> 1\n"
        "if (nibble & 1) delta += step >> 2\n"
        "if (nibble & 8) predictor -= delta\n"
        "else             predictor += delta\n"
        "predictor = clamp_i16(predictor)\n"
        "index = clamp(index + index_table[nibble & 7], 0, 88)"
    ))
    story.append(Spacer(1, 4))
    story.append(P("Tabela step padrão, 89 valores, do índice 0 ao 88.", "small"))
    story.append(code_block(
        "7 8 9 10 11 12 13 14 16 17 19 21 23 25 28 31 34 37 41 45\n"
        "50 55 60 66 73 80 88 97 107 118 130 143 157 173 190 209\n"
        "230 253 279 307 337 371 408 449 494 544 598 658 724 796\n"
        "876 963 1060 1166 1282 1411 1552 1707 1878 2066 2272 2499\n"
        "2749 3024 3327 3660 4026 4428 4871 5358 5894 6484 7132\n"
        "7845 8630 9493 10442 11487 12635 13899 15289 16818 18500\n"
        "20350 22385 24623 27086 29794 32767",
        size=7.2, lead=9.3,
    ))

    story.append(P("Microfone e USB", "h2"))
    story.append(P(
        "O INMP441 e o PCM5102A compartilham BCLK e WS, então os dois rodam a 48 kHz. "
        "Para a voz, a cada três amostras consecutivas o C3 grava a média das três. "
        "Isso é um filtro curto antes de descer para 16 kHz. Descartar duas e ficar com uma, sem média, gera alias e chiado. "
        "No mudo, o pacote continua saindo no tempo certo, com as 80 amostras em zero e o bit 0 de flags em 1."
    ))
    story.append(P(
        "No dongle, o USB anuncia reprodução e captura a 48 kHz, 16 bit. A captura é mono. "
        "Cada amostra de 16 kHz vira três amostras de 48 kHz por interpolação linear: "
        "entre a e b, sair a, (2a+b)/3 e (a+2b)/3. O Windows fica num formato só e não precisa reamostrar. "
        "No dispositivo de reprodução, desligar os aprimoramentos de áudio e fixar 48 kHz / 16 bit."
    ))

    story.append(P("4. Fiação digital", "h1"))
    story.append(P(
        "O ESP32-C3 tem um único I2S. Ele trabalha em full-duplex, mestre, padrão Philips, "
        "slot de 32 bit, 48 kHz. BCLK fica em 3,072 MHz, dentro da faixa do INMP441. "
        "O DAC usa os 16 bits de cima de cada slot de 32. Do microfone, também se usam os 16 bits de cima "
        "(o INMP441 entrega 24). GPIO8 e GPIO9 são de boot: não ligar nada neles. GPIO18 e GPIO19 são o USB do C3."
    ))
    story.append(table(
        ["Sinal", "GPIO", "Destino", "Cor"],
        [
            ["BCLK", "2", "PCM5102A BCK e INMP441 SCK, em paralelo", "Amarelo"],
            ["WS / LRCK", "3", "PCM5102A LRCK e INMP441 WS, em paralelo", "Verde"],
            ["DOUT, áudio do fone", "5", "PCM5102A DIN", "Azul claro"],
            ["DIN, voz", "4", "INMP441 SD", "Azul escuro"],
            ["ADC da bateria", "0", "Centro do divisor 100 kΩ + 100 kΩ", "Cinza"],
            ["Mudo do microfone", "6", "Chave para GND. Pull-up interno. Ativo em baixo", "Livre"],
            ["LED de enlace", "10", "LED e resistor de 1 kΩ até GND. Alto acende", "Livre"],
        ],
        [115, 48, 278, 70],
    ))
    story.append(Spacer(1, 6))
    story.append(callout(
        "Relógios e jumpers",
        "Os relógios saem do C3. Um resistor de 100 Ω em série, colado no pino que transmite, vai em BCLK, WS, DIN do DAC e SD do microfone. "
        "Do GPIO2 para 3,3 V, um resistor de 10 kΩ: o GPIO2 é pino de boot e não pode nascer segurado em baixo pelos fios. "
        "No PCM5102A, SCK no GND (o chip gera o clock a partir do BCK), FMT em I2S, XSMT em alto para sair do mudo, "
        "FLT em filtro normal, DEMP desligado. No INMP441, L/R no GND: isso seleciona o canal esquerdo. "
        "VDD do microfone é 3,3 V. Os fios de I2S ficam trançados com um terra e, no miolo, com menos de 15 cm. "
        "A haste do microfone pode ir a uns 20 cm se o trio for trançado com terra.",
        "note",
    ))
    story.append(Spacer(1, 6))
    story.append(P("Antena", "h2"))
    story.append(P(
        "A trilha em zigue-zague do SuperMini, na ponta oposta ao USB, aponta para o plástico da concha. "
        "Bateria, ímã do driver e qualquer fio ficam a pelo menos 10 mm dessa ponta. Nenhum fio atravessa a antena. "
        "No dongle, se a porta USB do PC ficar entre chapas de metal, um rabicho USB curto deixa a antena do S3 para fora."
    ))
    story.append(P(
        "Potência de transmissão em torno de 15 dBm. Sobe só se a sala não fechar o enlace. "
        "Protocolo 802.11 b/g/n. O modo long range fica desligado: ele baixa a taxa e aumenta a latência."
    ))

    story.append(P("5. Áudio analógico", "h1"))
    story.append(P(
        "O PCM5102A entrega cerca de 2,1 Vrms em 0 dBFS. O TPA6132, em 3,3 V e 32 Ω, corta bem antes disso. "
        "Um divisor fixo derruba uns 10 dB, e o potenciómetro passa a usar o curso inteiro. "
        "A ordem é DAC, divisor, potenciómetro, amplificador, driver. O driver não passa pelo potenciómetro."
    ))
    story.append(table(
        ["Ordem", "Peça", "Ligação"],
        [
            ["1", "PCM5102A LOUT e ROUT", "Saída de linha. Cerca de 2,1 Vrms em 0 dBFS."],
            ["2", "6,8 kΩ em série", "Um resistor por canal, na saída do DAC."],
            ["3", "3,3 kΩ para o GND", "Do ponto depois do 6,8 kΩ até o terra de áudio."],
            ["4", "Potenciómetro log", "Topo nesse ponto, extremo de baixo no GND, cursor é a saída."],
            ["5", "Entrada do TPA6132", "Cursor em INL e INR. Se o módulo não tiver 1 µF, colocar o capacitor aí."],
            ["6", "Saída do TPA6132", "OUTL no driver esquerdo. OUTR no fio branco do arco, driver direito."],
        ],
        [52, 145, 314],
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "A carga vista pelo DAC fica na casa de 10 kΩ, que é o que o PCM5102A pede. "
        "Se o módulo do TPA6132 já tiver capacitor de 1 µF na entrada, o cursor liga direto em INL e INR. "
        "Se não tiver, entra um capacitor de 1 µF entre o cursor e a entrada. "
        "O pino SD do TPA6132A2 em nível alto desliga a saída: amarrar SD em GND se a placa não fizer isso."
    ))
    story.append(P(
        "A roda original serve se for um potenciómetro passivo de dois eixos. No multímetro, a resistência entre os extremos "
        "de cada eixo fica parada enquanto a roda gira, e a resistência do cursor até um extremo muda. "
        "Valor típico: 10 kΩ a 50 kΩ, curva logarítmica. Se a placa tiver um circuito integrado amplificando, "
        "não empilhar outro amplificador: ou fica a placa original, ou fica o TPA6132, nunca os dois em série."
    ))
    story.append(P("Terra e arco", "h2"))
    story.append(P(
        "O terra de áudio encontra o terra de potência num ponto só, no GND do buck-boost. "
        "O retorno de cada driver vai ao GND do TPA6132, e de lá um fio único segue até esse ponto. "
        "O retorno do driver direito não se mistura com o negativo da bateria ao longo do arco."
    ))
    story.append(table(
        ["Fio no arco", "Função", "Cor"],
        [
            ["1", "Positivo das duas LiPo em paralelo", "Vermelho"],
            ["2", "Negativo das baterias. Não é retorno de áudio", "Preto"],
            ["3", "Driver direito, sinal, saindo do TPA6132 OUTR", "Branco"],
            ["4", "Retorno do driver direito, até o GND do TPA6132", "Marrom"],
        ],
        [70, 341, 100],
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "O driver esquerdo fica na própria concha: OUTL no positivo do alto-falante e o retorno no GND do TPA6132. "
        "Fio de silicone 30 AWG, com folga na articulação. O cabo não pode ser esmagado quando o arco regula a altura. "
        "A chave de mudo, se na origem cortava o alto-falante, deixa de cortar áudio e passa a fechar o GPIO6 em GND."
    ))

    story.append(P("6. Lista de material", "h1"))
    story.append(table(
        ["Qtd", "Peça", "Nota"],
        [
            ["1", "BlackShark V2 X", "Doador dos drivers, do arco, da roda e da chave de mudo."],
            ["1", "ESP32-C3 SuperMini", "Fica no fone. USB só para gravar."],
            ["1", "ESP32-S3 com USB nativo", "Dongle. Placa que só tem CH340 não serve."],
            ["1", "INMP441", "Microfone I2S. Alimentar em 3,3 V. L/R no GND."],
            ["1", "Módulo PCM5102A", "SCK no GND. XSMT em alto."],
            ["1", "Módulo TPA6132A2", "SD em GND para o amplificador ficar ligado."],
            ["1", "TP4056 com USB-C e DW01", "Única proteção do pack. Carga em torno de 1 A."],
            ["1", "Buck-boost 3,3 V, 800 mA ou mais", "Pololu S7V8F3, TPS63020 ou TPS63802."],
            ["2", "LiPo 800 mAh sem PCM próprio", "Igualar a tensão antes de colocar em paralelo."],
            ["1", "Chave slide e polyfuse 2 A", "Em série no positivo, depois da proteção."],
            ["1", "Eletrolítico 470 µF, 10 V", "Nos pinos 3V3 e GND do C3."],
            ["5", "Cerâmico 100 nF", "C3, DAC, amplificador, microfone e GPIO0."],
            ["2 + 2", "100 kΩ, e 6,8 kΩ com 3,3 kΩ", "Divisor da bateria. Divisor de linha do DAC, um jogo por canal."],
            ["4 + 1 + 1", "100 Ω, 10 kΩ e 1 kΩ", "Série do I2S, pull-up do GPIO2, resistor do LED."],
            ["—", "Silicone 30 AWG, Kapton, fita dupla face", "Arco, isolamento e fixação das células."],
        ],
        [62, 175, 274],
    ))

    story.append(P("7. Alimentação", "h1"))
    story.append(callout(
        "Bateria na cabeça",
        "Duas células em paralelo viram um pack só de 1S. Igualar as tensões em 0,05 V ou menos antes de unir, "
        "sem faísca e sem aquecimento. Células sem placa de proteção própria, e uma proteção só no conjunto: "
        "o módulo TP4056 com DW01. Carregar sempre com a chave desligada e com o fone fora da cabeça. "
        "Se a concha esquentar, parar. Não furar, não dobrar e não deixar a LiPo ao sol dentro do carro.",
        "stop",
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "O SuperMini espera 5 V no VIN. Uma LiPo anda entre 4,2 V e cerca de 3,3 V. "
        "No meio da descarga o regulador da placa já não entrega 3,3 V, e o pico do Wi-Fi, perto de 300 mA, "
        "reinicia o chip ou chia no áudio. O caminho certo é um buck-boost que aceita 3,0 V a 4,2 V "
        "e segura 3,3 V com pelo menos 800 mA. Exemplos dessa classe: Pololu S7V8F3, ou um módulo com TPS63020 ou TPS63802. "
        "A saída entra no pino 3V3 do C3, no DAC, no microfone e no TPA6132. O pino VIN fica sem fio."
    ))
    story.append(table(
        ["Nó", "Liga em"],
        [
            ["Positivo das duas células", "TP4056 BAT+ e o divisor de 100 kΩ do GPIO0."],
            ["Negativo das células", "TP4056 BAT−, GND do buck-boost e o ponto único de terra."],
            ["Saída protegida, lado da bateria", "Polyfuse de 2 A. A chave vem depois do polyfuse."],
            ["Depois da chave", "Entrada do buck-boost. Com a chave aberta, o pack ainda carrega."],
            ["Saída de 3,3 V", "Pino 3V3 do C3, PCM5102A, INMP441 e TPA6132. 100 nF em cada um."],
            ["470 µF, 10 V", "Direto nos pinos 3V3 e GND do C3. A faixa do capacitor fica no GND."],
            ["USB-C", "Só no TP4056. A chave fica desligada durante a carga."],
        ],
        [175, 336],
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "No primeiro ligar, a tensão no 3V3 tem de ficar entre 3,20 V e 3,40 V enquanto o C3 transmite. "
        "Abaixo de 3,15 V o regulador não serve: trocar antes de fechar a concha. "
        "Autonomia, em ordem de grandeza: o conjunto puxa cerca de 120 a 180 mA da bateria em volume moderado. "
        "1600 mAh nessa faixa dá algo entre 6 e 10 horas. Volume alto e transmissão no máximo encurtam para perto de 6 horas."
    ))
    story.append(P("Medir a bateria", "h2"))
    story.append(P(
        "Do positivo do pack, 100 kΩ até o GPIO0. Do GPIO0, 100 kΩ até o GND. Um capacitor de 100 nF do GPIO0 ao GND. "
        "Atenuação do ADC em 12 dB e calibração de fábrica do ESP-IDF. A tensão do pack é o dobro da tensão no pino. "
        "Não usar a conta 3,3 V / 4095: ela erra fácil um décimo de volt. Ler o ADC uma vez por segundo, média de 16 amostras, "
        "e copiar o porcentual já pronto em todos os pacotes de subida. O GPIO0 no C3 é ADC1 e não é pino de boot."
    ))
    story.append(table(
        ["Tensão do pack", "Porcentagem"],
        [
            ["4,15 V ou mais", "100"],
            ["4,00 V", "85"],
            ["3,90 V", "70"],
            ["3,80 V", "55"],
            ["3,70 V", "40"],
            ["3,55 V", "20"],
            ["3,40 V", "8"],
            ["3,30 V ou menos", "0. O LED pisca rápido abaixo de 15%. A proteção abre um pouco depois."],
        ],
        [140, 371],
    ))
    story.append(Spacer(1, 4))
    story.append(P("Interpolar entre as linhas. A curva da LiPo é achatada no meio; a tabela já acompanha isso.", "caption"))
    story.append(callout(
        "Gravar firmware",
        "Chave desligada. Só então o USB do SuperMini entra no computador. Gravar. Tirar o USB. "
        "Só depois a chave pode ligar. Com o buck-boost e o USB ao mesmo tempo, o regulador da placa e o 3,3 V externo disputam o trilho. "
        "O dongle precisa do USB nativo do S3, GPIO19 e GPIO20. Placa que só tem CH340 não vira dispositivo de áudio.",
        "warn",
    ))

    story.append(P("8. Montagem", "h1"))
    story.append(P(
        "Concha direita: uma LiPo de 800 mAh, fita dupla face que amortece vibração, e o driver original. "
        "Concha esquerda: a outra LiPo, TP4056, polyfuse, chave, buck-boost, C3, PCM5102A, TPA6132 e a placa da roda de volume. "
        "Isolar as placas com fita Kapton antes de fechar. A haste do microfone continua na esquerda, com o INMP441."
    ))
    story.append(table(
        ["Etapa", "O que fazer", "Aceite"],
        [
            ["1. Células",
             "Medir as duas LiPo. Igualar até a diferença ficar em 0,05 V ou menos. Unir em paralelo e não separar mais.",
             "Ao unir, não há faísca nem aquecimento. As tensões ficam iguais."],
            ["2. Carga",
             "TP4056 com DW01 no pack, antes da chave. USB-C só nesse módulo. Corrente do módulo em torno de 1 A.",
             "Carrega com a chave desligada. O módulo termina a carga e não fica quente demais para o dedo."],
            ["3. 3,3 V",
             "Buck-boost depois da chave. Medir a saída em vazio e de novo com o C3 transmitindo.",
             "3,20 V a 3,40 V nos dois casos. Abaixo de 3,15 V, trocar o módulo."],
            ["4. Arco",
             "Quatro fios de silicone 30 AWG, com folga na articulação.",
             "Continuidade de ponta a ponta e as conchas giram sem prender o cabo."],
            ["5. Relógios",
             "I2S em paralelo, resistores de 100 Ω, pull-up de 10 kΩ no GPIO2, jumpers do PCM5102A e L/R do microfone no GND.",
             "SCK do DAC e L/R do microfone medem 0 Ω até o GND. GPIO2 mede cerca de 3,3 V com o chip em reset."],
            ["6. Analógico",
             "Divisor 6,8 kΩ / 3,3 kΩ, potenciómetro, TPA6132, drivers. Terra em estrela. SD do amp em GND.",
             "Tom de 1 kHz a −6 dBFS limpo nos dois ouvidos, do mínimo ao máximo da roda."],
            ["7. Rádio",
             "Gravar C3 e S3 com o MAC um do outro, canal 6, sem roteador, power save desligado.",
             "O PC vê alto-falante e microfone. O LED fica aceso com enlace. Mudo zera a voz e não corta a música."],
            ["8. Prova",
             "Clique reproduzido no fone e captado pela haste, gravado de volta no PC. Antena longe de metal.",
             "O atraso do clique gravado fica entre 55 e 85 ms. Uma sala, sem buracos longos."],
        ],
        [78, 230, 203],
    ))

    story.append(P("9. Firmware, em contrato curto", "h1"))
    story.append(P(
        "ESP-IDF 5. O I2S do C3 é mestre, Philips, 48 kHz, slot de 32 bit, full-duplex, dois buffers DMA de 240 quadros. "
        "O S3 usa USB Audio Class 1.0, full-speed: reprodução estéreo e captura mono, ambos a 48 kHz e 16 bit. "
        "Os MAC vão gravados um no firmware do outro. Canal 6. Modo estação, sem access point. "
        "WIFI_PS_NONE. Sem protocolo long range."
    ))
    story.append(table(
        ["LED no GPIO10", "Significado"],
        [
            ["Aceso", "Enlace vivo: chegou pacote de descida neste quadro."],
            ["Pisca devagar", "Procurando o dongle."],
            ["Pisca rápido", "Bateria abaixo de 15%."],
        ],
        [130, 381],
    ))
    story.append(Spacer(1, 6))
    story.append(P(
        "A prova de atraso mede ida e volta: o clique sai pelo driver, a haste ouve e o USB devolve ao PC. "
        "Esse número inclui descida, subida e o caminho do ar entre o driver e o microfone. "
        "O jogador, no caminho do jogo até o ouvido, fica na metade útil disso, 25 a 40 ms. "
        "55 a 85 ms na gravação do clique é o aceite. Bem acima de 85 ms, o buffer de espera passou de dois quadros "
        "ou o Windows está reamostrando."
    ))

    story.append(P("Ficha de comissionamento", "h2"))
    story.append(table(
        ["Campo", "Valor medido"],
        [
            ["MAC do ESP32-C3", " "],
            ["MAC do ESP32-S3", " "],
            ["Canal", "6"],
            ["Tensão da LiPo direita, antes do paralelo", "mV"],
            ["Tensão da LiPo esquerda, antes do paralelo", "mV"],
            ["3,3 V com o rádio transmitindo", "V"],
            ["Atraso do clique, ida e volta", "ms"],
            ["Data da montagem", " "],
        ],
        [280, 231],
    ))
    story.append(P("10. Liberar o fone", "h1"))
    story.append(P(
        "O fone só fecha depois desta lista. Cada linha é uma medida, não uma impressão."
    ))
    story.append(table(
        ["Prova", "Pronto quando"],
        [
            ["Gravação", "USB do C3 só com a chave desligada. USB fora antes de ligar a bateria."],
            ["3,3 V", "Entre 3,20 V e 3,40 V com o rádio transmitindo."],
            ["Som", "Tom de 1 kHz a −6 dBFS limpo nos dois drivers, do mínimo ao máximo da roda."],
            ["Mudo", "A chave zera a voz no PC e a música continua nos dois ouvidos."],
            ["Atraso", "O clique gravado de volta fica entre 55 e 85 ms."],
            ["Antena", "Nada de metal, bateria ou fio sobre a ponta do SuperMini."],
            ["Calor", "Dez minutos de áudio e a concha continua confortável na mão."],
        ],
        [80, 431],
    ))
    story.append(Spacer(1, 8))
    story.append(P(
        "Projeto Headset Wireless · Revisão B · documento de montagem. "
        "A revisão anterior continua válida só como referência mecânica das conchas.",
        "small",
    ))

    doc = SimpleDocTemplate(
        OUT,
        pagesize=A4,
        leftMargin=16 * mm,
        rightMargin=16 * mm,
        topMargin=16 * mm,
        bottomMargin=16 * mm,
        title="Manual de Engenharia - Headset Wireless ESP32 - Revisão B",
        author="Projeto Headset Wireless",
        subject="Arquitetura corrigida: áudio ESP-NOW, alimentação e montagem",
    )
    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
    print(OUT)


if __name__ == "__main__":
    build()
