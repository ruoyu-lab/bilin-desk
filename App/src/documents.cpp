#include "documents.h"

#include <QFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>

QString Documents::text(const QString &path) const
{
    QFile document(QStringLiteral(":/") + path);
    if (!document.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("程序里没有这份文字：%s", qUtf8Printable(path));
        return {};
    }
    return QString::fromUtf8(document.readAll());
}

void Documents::typeset(QQuickTextDocument *document) const
{
    QTextDocument *text = document->textDocument();
    QTextCursor cursor(text);
    cursor.beginEditBlock();
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        const int heading = format.headingLevel();
        const bool first = block == text->begin();
        if (heading > 0) {
            format.setTopMargin(first ? 0 : 22);
            format.setBottomMargin(8);
            format.setLineHeight(130, QTextBlockFormat::ProportionalHeight);
        } else {
            format.setTopMargin(0);
            format.setBottomMargin(block.textList() ? 6 : 12);
            format.setLineHeight(165, QTextBlockFormat::ProportionalHeight);
        }
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
        if (heading > 0) {
            // 读 Markdown 时标题按级别放大了好几号：二级标题只比正文大一点
            QTextCharFormat chars;
            chars.setProperty(QTextFormat::FontSizeAdjustment, 0);
            chars.setProperty(QTextFormat::FontPixelSize, heading <= 2 ? 16 : 14);
            chars.setFontWeight(QFont::DemiBold);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(chars);
        }
    }
    cursor.endEditBlock();
}
