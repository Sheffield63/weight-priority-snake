/**
 * @file make_icon.cpp
 * @brief One-shot tool: renders the snake-game app icon and writes a
 *        multi-size ICO (16/24/32/48/64/128/256) as resources/app.ico.
 *
 * Not part of the main build. Compile & run manually, e.g.:
 *   g++ -std=c++11 -fPIC tools/make_icon.cpp -o build/make_icon.exe \
 *       -I D:\Qt\5.15.2\mingw81_64\include -I D:\Qt\5.15.2\mingw81_64\include\QtCore \
 *       -I D:\Qt\5.15.2\mingw81_64\include\QtGui -L D:\Qt\5.15.2\mingw81_64\lib \
 *       -lQt5Core -lQt5Gui
 * (run with D:\Qt\5.15.2\mingw81_64\bin on PATH)
 *
 * ICO packing: Vista+ PNG-compressed entries are emitted manually so we do
 * not depend on Qt's ICO write plugin supporting multi-frame output.
 */

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QBuffer>
#include <QFile>
#include <QByteArray>

static QImage drawIcon(int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = size / 256.0;

    // Dark rounded tile background (subtle vertical gradient)
    QLinearGradient bg(0, 0, 0, size);
    bg.setColorAt(0.0, QColor(46, 66, 46));
    bg.setColorAt(1.0, QColor(14, 20, 14));
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0, 0, size, size), 56 * s, 56 * s);

    // Inner hairline border
    QPen borderPen(QColor(255, 255, 255, 26), 3 * s);
    p.setBrush(Qt::NoBrush);
    p.setPen(borderPen);
    p.drawRoundedRect(QRectF(1.5 * s, 1.5 * s, size - 3 * s, size - 3 * s), 55 * s, 55 * s);

    // Snake body: zigzag path, rounded caps/joins
    QPainterPath body;
    body.moveTo(55 * s, 200 * s);
    body.lineTo(200 * s, 200 * s);
    body.lineTo(200 * s, 130 * s);
    body.lineTo(55 * s, 130 * s);
    body.lineTo(55 * s, 60 * s);
    body.lineTo(200 * s, 60 * s);
    QPen snakePen(QColor(76, 175, 80), 46 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(snakePen);
    p.drawPath(body);

    // Head: brighter circle with eyes, facing right
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(102, 187, 106));
    p.drawEllipse(QPointF(200 * s, 60 * s), 26 * s, 26 * s);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(183 * s, 49 * s), 9 * s, 9 * s);
    p.drawEllipse(QPointF(215 * s, 49 * s), 9 * s, 9 * s);
    p.setBrush(QColor(20, 30, 20));
    p.drawEllipse(QPointF(184 * s, 50 * s), 4.5 * s, 4.5 * s);
    p.drawEllipse(QPointF(216 * s, 50 * s), 4.5 * s, 4.5 * s);

    // Food: red dot with highlight
    p.setBrush(QColor(229, 57, 53));
    p.drawEllipse(QPointF(127 * s, 165 * s), 21 * s, 21 * s);
    p.setBrush(QColor(255, 138, 128));
    p.drawEllipse(QPointF(119 * s, 157 * s), 7 * s, 7 * s);

    return img;
}

static QByteArray pngBytes(const QImage& img)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return buf.data();
}

static void appendLE16(QByteArray& out, quint16 v)
{
    out.append(static_cast<char>(v & 0xff));
    out.append(static_cast<char>((v >> 8) & 0xff));
}

static void appendLE32(QByteArray& out, quint32 v)
{
    out.append(static_cast<char>(v & 0xff));
    out.append(static_cast<char>((v >> 8) & 0xff));
    out.append(static_cast<char>((v >> 16) & 0xff));
    out.append(static_cast<char>((v >> 24) & 0xff));
}

// No QGuiApplication needed: QImage painting is raster-only and we draw no
// text, so no QPA platform plugin is required.
int main()
{

    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    const int n = static_cast<int>(sizeof(sizes) / sizeof(sizes[0]));

    QByteArray payloads[7];
    for (int i = 0; i < n; ++i) {
        payloads[i] = pngBytes(drawIcon(sizes[i]));
    }

    QByteArray ico;
    // ICONDIR
    appendLE16(ico, 0);        // reserved
    appendLE16(ico, 1);        // type: icon
    appendLE16(ico, n);        // count
    // ICONDIRENTRY x N
    quint32 offset = 6 + 16 * n;
    for (int i = 0; i < n; ++i) {
        const int s = sizes[i];
        ico.append(static_cast<char>(s == 256 ? 0 : s));  // width (0 = 256)
        ico.append(static_cast<char>(s == 256 ? 0 : s));  // height
        ico.append(static_cast<char>(0));                 // palette count
        ico.append(static_cast<char>(0));                 // reserved
        appendLE16(ico, 1);                               // planes
        appendLE16(ico, 32);                              // bits per pixel
        appendLE32(ico, payloads[i].size());
        appendLE32(ico, offset);
        offset += payloads[i].size();
    }
    // PNG payloads
    for (int i = 0; i < n; ++i) {
        ico.append(payloads[i]);
    }

    QFile out(QStringLiteral("resources/app.ico"));
    if (!out.open(QIODevice::WriteOnly)) {
        return 1;
    }
    out.write(ico);
    out.close();
    return 0;
}
