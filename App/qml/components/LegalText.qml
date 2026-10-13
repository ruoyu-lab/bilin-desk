import QtQuick

// 服务协议、隐私政策的正文（App/legal 里的 Markdown），不带最上面的标题：标题由放它的地方显示
TextEdit {
    // terms | privacy
    property string kind

    readOnly: true
    selectByMouse: true
    textFormat: TextEdit.MarkdownText
    wrapMode: TextEdit.Wrap
    text: kind.length > 0 ? Documents.text("legal/" + kind + ".md").replace(/^# .*\n+/, "") : ""
    font.pixelSize: 13
    color: Theme.text
    selectionColor: Theme.selection
    selectedTextColor: Theme.text
    // Markdown 读进来以后排版（行距、段落、标题前后的留白）。排版改了文档又会触发 textChanged：排版时的那几次不管
    property bool typesetting: false
    onTextChanged: {
        if (typesetting)
            return
        typesetting = true
        Documents.typeset(textDocument)
        typesetting = false
    }
}
