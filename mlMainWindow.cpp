/*
*
* Copyright 2016 Activision Publishing, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include "stdafx.h"

#include "mlMainWindow.h"

#include <functional>
#include <iomanip>
#include <sstream>
#include <TlHelp32.h>
#include <utility>

#pragma comment(lib, "steam_api64.lib")

const int AppId = 311210;

const char* gLanguages[] = { "english", "french", "italian", "spanish", "german", "portuguese", "russian", "polish", "japanese", "traditionalchinese", "simplifiedchinese", "englisharabic" };
const char* gTags[] = { "Animation", "Audio", "Character", "Map", "Mod", "Mode", "Model", "Multiplayer", "Scorestreak", "Skin", "Specialist", "Texture", "UI", "Vehicle", "Visual Effect", "Weapon", "WIP", "Zombies" };
dvar_s gDvars[] = {
					{"ai_disableSpawn", "Disable AI from spawning", DVAR_VALUE_BOOL},
					{"developer", "Run developer mode", DVAR_VALUE_INT, 0, 2},
					{"g_password", "Password for your server", DVAR_VALUE_STRING},
					{"logfile", "Console log information written to current fs_game", DVAR_VALUE_INT, 0, 2},
					{"scr_mod_enable_devblock", "Developer blocks are executed in mods ", DVAR_VALUE_BOOL},
					{"connect", "Connect to a specific server", DVAR_VALUE_STRING, NULL, NULL, true},
					{"set_gametype", "Set a gametype to load with map", DVAR_VALUE_STRING, NULL, NULL, true},
					{"splitscreen", "Enable splitscreen", DVAR_VALUE_BOOL},
					{"splitscreen_playerCount", "Allocate the number of instances for splitscreen", DVAR_VALUE_INT, 0, 2}
				 };

enum ThemeModeValue
{
	ThemeOriginalUpdated,
	ThemeOriginalClassic,
	ThemeDarkModern
};

static QString ThemeModeToSettingsValue(int ThemeModeValue)
{
	switch (ThemeModeValue)
	{
	case ThemeOriginalClassic:
		return "original-classic";
	case ThemeDarkModern:
		return "dark-modern";
	case ThemeOriginalUpdated:
	default:
		return "original-updated";
	}
}

static int ThemeModeFromSettings(const QSettings& Settings)
{
	const QString SavedThemeMode = Settings.value("ThemeMode", "").toString().trimmed().toLower();
	if (SavedThemeMode == "original-classic")
		return ThemeOriginalClassic;
	if (SavedThemeMode == "dark-modern")
		return ThemeDarkModern;
	if (SavedThemeMode == "original-updated")
		return ThemeOriginalUpdated;

	if (Settings.contains("UseDarkTheme"))
		return Settings.value("UseDarkTheme", false).toBool() ? ThemeOriginalUpdated : ThemeDarkModern;

	return ThemeOriginalUpdated;
}

static bool ThemeUsesUpdatedChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeOriginalUpdated;
}

static bool ThemeUsesClassicChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeOriginalClassic;
}

static bool ThemeUsesDarkModernChrome(int ThemeModeValue)
{
	return ThemeModeValue == ThemeDarkModern;
}

static int TreeRowHeightForTheme(int ThemeModeValue, bool CompactRow)
{
	if (ThemeUsesClassicChrome(ThemeModeValue))
		return CompactRow ? 22 : 22;
	return CompactRow ? 36 : 46;
}

static const char* kThemeProfileSettingKey = "SelectedThemeProfile";
static const char* kThemeProfilesGroup = "ThemeProfiles";

static QString BuiltInThemeProfileId(int ThemeModeValue)
{
	switch (ThemeModeValue)
	{
	case ThemeOriginalClassic:
		return "original-classic";
	case ThemeDarkModern:
		return "dark-modern";
	case ThemeOriginalUpdated:
	default:
		return "original-updated";
	}
}

static int ThemeModeFromProfileId(const QString& ThemeProfileId)
{
	if (ThemeProfileId == "original-classic")
		return ThemeOriginalClassic;
	if (ThemeProfileId == "dark-modern")
		return ThemeDarkModern;
	return ThemeOriginalUpdated;
}

static QString ThemeProfileDisplayNameForBuiltInId(const QString& ThemeProfileId)
{
	if (ThemeProfileId == "original-classic")
		return "Original Classic";
	if (ThemeProfileId == "dark-modern")
		return "Dark Modern";
	return "Original Updated";
}

static QStringList ThemeProfileSettingKeys()
{
	return QStringList()
		<< "ThemeMode"
		<< "AccentColor"
		<< "ShowItemTypeTags"
		<< "CustomStylesheet"
		<< "ConsoleStyle"
		<< "AssetTreeBackgroundImage"
		<< "AssetTreeBackgroundOpacity"
		<< "LogBackgroundImage"
		<< "LogBackgroundOpacity"
		<< "LauncherLayout"
		<< "LogColors/Default"
		<< "LogColors/Command"
		<< "LogColors/Info"
		<< "LogColors/Launch"
		<< "LogColors/Success"
		<< "LogColors/Warning"
		<< "LogColors/Error";
}

static QVariantMap DefaultThemeProfileValues(const QString& ThemeProfileId)
{
	QVariantMap Values;
	Values.insert("ThemeMode", ThemeModeToSettingsValue(ThemeModeFromProfileId(ThemeProfileId)));
	Values.insert("AccentColor", "#ff8a2a");
	Values.insert("ShowItemTypeTags", true);
	Values.insert("CustomStylesheet", "");
	Values.insert("ConsoleStyle", "improved");
	Values.insert("AssetTreeBackgroundImage", "");
	Values.insert("AssetTreeBackgroundOpacity", 100);
	Values.insert("LogBackgroundImage", "");
	Values.insert("LogBackgroundOpacity", 100);
	Values.insert("LauncherLayout", "modern");
	Values.insert("LogColors/Default", "#d7dce2");
	Values.insert("LogColors/Command", "#7dcfff");
	Values.insert("LogColors/Info", "#eef1f4");
	Values.insert("LogColors/Launch", "#c792ea");
	Values.insert("LogColors/Success", "#6ee7a8");
	Values.insert("LogColors/Warning", "#ffcf70");
	Values.insert("LogColors/Error", "#ff7a7a");
	return Values;
}

static QString SanitizedThemeProfileId(const QString& DisplayName)
{
	QString ThemeId = DisplayName.trimmed().toLower();
	ThemeId.replace(QRegularExpression("[^a-z0-9]+"), "-");
	while (ThemeId.contains("--"))
		ThemeId.replace("--", "-");
	ThemeId.remove(QRegularExpression("^-+|-+$"));
	if (ThemeId.isEmpty())
		ThemeId = "custom-theme";
	return ThemeId;
}

enum mlItemType
{
	ML_ITEM_UNKNOWN,
	ML_ITEM_MAP,
	ML_ITEM_MOD,
	ML_ITEM_MOD_GROUP
};

enum mlItemDataRole
{
	ML_ITEM_CONTAINER_ROLE = Qt::UserRole + 1,
	ML_ITEM_NAME_ROLE,
	ML_ITEM_FAVORITE_ROLE,
	ML_ITEM_CHECKSTATE_ROLE
};

enum mlLogDataRole
{
	ML_LOG_TEXT_ROLE = Qt::UserRole + 100,
	ML_LOG_IS_HEADER_ROLE,
	ML_LOG_EXPANDED_ROLE
};

static QString StripTreyarchColorCodes(const QString& Title)
{
	QString CleanTitle = Title;
	CleanTitle.remove(QRegularExpression("\\^[0-9]"));
	return CleanTitle.trimmed();
}

static QString SteamDescriptionForUpload(const QString& BriefingDescription, const QString& SteamDescription)
{
	return SteamDescription.trimmed().isEmpty() ? BriefingDescription : SteamDescription;
}

static bool IsGameLaunchCommand(const QString& ProgramPath, const QStringList& Args)
{
	const QString FileName = QFileInfo(ProgramPath).fileName();
	return FileName.compare("BlackOps3.exe", Qt::CaseInsensitive) == 0
		|| (FileName.compare("Steam.exe", Qt::CaseInsensitive) == 0 && Args.contains("-applaunch") && Args.contains(QString::number(AppId)));
}

struct GameWindowSearchState
{
	DWORD ProcessId;
	bool HasVisibleWindow;
};

class BackgroundDropLineEdit : public QLineEdit
{
public:
	BackgroundDropLineEdit(const QString& Text = QString(), QWidget* Parent = NULL)
		: QLineEdit(Text, Parent)
	{
		setAcceptDrops(true);
	}

protected:
	void dragEnterEvent(QDragEnterEvent* Event)
	{
		if (Event->mimeData()->hasUrls())
			Event->acceptProposedAction();
		else
			QLineEdit::dragEnterEvent(Event);
	}

	void dropEvent(QDropEvent* Event)
	{
		if (Event->mimeData()->hasUrls() && Event->mimeData()->urls().count())
		{
			const QString LocalPath = Event->mimeData()->urls().first().toLocalFile();
			if (!LocalPath.isEmpty())
			{
				setText(QDir::toNativeSeparators(LocalPath));
				Event->acceptProposedAction();
				return;
			}
		}

		QLineEdit::dropEvent(Event);
	}
};

class HoverRevealWidget : public QWidget
{
public:
	HoverRevealWidget(QWidget* Parent = NULL)
		: QWidget(Parent), mRevealWidget(NULL)
	{
		setAttribute(Qt::WA_Hover, true);
	}

	void SetRevealWidget(QWidget* Widget)
	{
		mRevealWidget = Widget;
		if (mRevealWidget)
			mRevealWidget->setVisible(false);
	}

protected:
	void resizeEvent(QResizeEvent* Event)
	{
		QWidget::resizeEvent(Event);
	}

	void enterEvent(QEnterEvent* Event)
	{
		setProperty("hovered", true);
		style()->unpolish(this);
		style()->polish(this);
		update();
		if (mRevealWidget)
			mRevealWidget->setVisible(true);
		QWidget::enterEvent(Event);
	}

	void leaveEvent(QEvent* Event)
	{
		setProperty("hovered", false);
		style()->unpolish(this);
		style()->polish(this);
		update();
		if (mRevealWidget)
			mRevealWidget->setVisible(false);
		QWidget::leaveEvent(Event);
	}

private:
	QWidget* mRevealWidget;
};

static void UpdateBackgroundPreviewLabel(QLabel* PreviewLabel, const QString& FileName)
{
	if (!PreviewLabel)
		return;

	PreviewLabel->setText("None");
	PreviewLabel->setPixmap(QPixmap());

	const QString NormalizedPath = FileName.trimmed();
	if (NormalizedPath.isEmpty())
		return;

	QString PreviewPath = NormalizedPath;
	if (NormalizedPath.startsWith("qrc:/"))
		PreviewPath = ":" + NormalizedPath.mid(4);

	const QPixmap SourcePixmap(PreviewPath);
	if (SourcePixmap.isNull())
	{
		PreviewLabel->setText("Invalid");
		return;
	}

	PreviewLabel->setPixmap(SourcePixmap.scaled(PreviewLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

static QString SanitizedFileName(QString Value)
{
	Value = Value.trimmed();
	Value.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
	while (Value.contains("__"))
		Value.replace("__", "_");
	Value = Value.trimmed();
	if (Value.isEmpty())
		Value = "default";
	return Value;
}

static QString HumanizedVersionLabel(const QString& Value)
{
	QString Label = Value.trimmed();
	if (Label.isEmpty())
		return Label;
	Label.replace('_', ' ');
	Label.replace('-', ' ');
	Label.replace(QRegularExpression("\\s+"), " ");
	return Label.trimmed();
}

static bool EditJsonTextDialog(QWidget* Parent, const QString& FilePath, const QString& Title)
{
	QFile File(FilePath);
	if (!File.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(Parent, Title, QString("Unable to open '%1'.").arg(FilePath));
		return false;
	}

	QDialog Dialog(Parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(Title);
	Dialog.resize(880, 680);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QLabel* PathLabel = new QLabel(QDir::toNativeSeparators(FilePath), &Dialog);
	PathLabel->setWordWrap(true);
	Layout->addWidget(PathLabel);

	QTextEdit* Editor = new QTextEdit(&Dialog);
	Editor->setAcceptRichText(false);
	Editor->setPlainText(QString::fromUtf8(File.readAll()));
	Layout->addWidget(Editor, 1);

	QDialogButtonBox* Buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(Buttons);
	QObject::connect(Buttons, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	QObject::connect(Buttons, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	QJsonParseError ParseError;
	const QByteArray UpdatedBytes = Editor->toPlainText().toUtf8();
	QJsonDocument Parsed = QJsonDocument::fromJson(UpdatedBytes, &ParseError);
	if (ParseError.error != QJsonParseError::NoError)
	{
		QMessageBox::warning(Parent, Title, QString("Invalid JSON: %1").arg(ParseError.errorString()));
		return false;
	}

	QFile OutFile(FilePath);
	if (!OutFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		QMessageBox::warning(Parent, Title, QString("Unable to write '%1'.").arg(FilePath));
		return false;
	}

	OutFile.write(Parsed.toJson(QJsonDocument::Indented));
	return true;
}

static QString SteamMarkupToHtml(QString Text)
{
	const QRegularExpression::PatternOptions MarkupOptions =
		QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption;
	Text = Text.toHtmlEscaped();
	Text.replace(QRegularExpression("\\[b\\](.*?)\\[/b\\]", MarkupOptions), "<b>\\1</b>");
	Text.replace(QRegularExpression("\\[i\\](.*?)\\[/i\\]", MarkupOptions), "<i>\\1</i>");
	Text.replace(QRegularExpression("\\[u\\](.*?)\\[/u\\]", MarkupOptions), "<u>\\1</u>");
	Text.replace(QRegularExpression("\\[strike\\](.*?)\\[/strike\\]", MarkupOptions), "<span style=\"text-decoration:line-through;\">\\1</span>");
	Text.replace(QRegularExpression("\\[url=(.*?)\\](.*?)\\[/url\\]", MarkupOptions), "<a href=\"\\1\">\\2</a>");
	Text.replace(QRegularExpression("\\[url\\](.*?)\\[/url\\]", MarkupOptions), "<a href=\"\\1\">\\1</a>");
	Text.replace(QRegularExpression("\\[img\\](.*?)\\[/img\\]", MarkupOptions), "<div style=\"margin:10px 0;\"><a href=\"\\1\">Open image</a><br/><span style=\"color:#8d96a0; font-size:11px;\">\\1</span></div>");
	Text.replace(QRegularExpression("\\[h1\\](.*?)\\[/h1\\]", MarkupOptions), "<h3>\\1</h3>");
	Text.replace(QRegularExpression("\\[quote\\](.*?)\\[/quote\\]", MarkupOptions), "<blockquote>\\1</blockquote>");
	Text.replace("[*]", "<li>");
	Text.replace(QRegularExpression("\\[list\\](.*?)\\[/list\\]", MarkupOptions), "<ul>\\1</ul>");
	Text.replace("\n", "<br/>");
	return QString("<html><body style=\"margin:8px; line-height:1.45;\">%1</body></html>").arg(Text);
}

static QString NormalizedStoredColor(QString Value)
{
	Value = Value.trimmed();
	if (Value.isEmpty())
		return QString();

	const QColor Parsed(Value);
	return Parsed.isValid() ? Parsed.name(QColor::HexRgb) : QString();
}

static bool ConfirmDestructiveActionTwice(QWidget* Parent, const QString& Title, const QString& TargetLabel, const QString& DetailText)
{
	if (QMessageBox::warning(Parent, Title, QString("You are about to delete %1.\n\n%2").arg(TargetLabel, DetailText), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return false;

	return QMessageBox::warning(Parent, Title, QString("Are you sure you want to delete %1?").arg(TargetLabel), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

enum LogMessageKind
{
	LogMessageDefault,
	LogMessageMuted,
	LogMessageCommand,
	LogMessageInfo,
	LogMessageSuccess,
	LogMessageWarning,
	LogMessageError,
	LogMessageLaunch,
	LogMessageAccent
};

static LogMessageKind DetectLogMessageKind(const QString& Output);

enum LogFilterKind
{
	LogFilterLeakMod,
	LogFilterPhilLibX,
	LogFilterGdtDb,
	LogFilterLinking,
	LogFilterCommands,
	LogFilterWarnings,
	LogFilterErrors
};

enum OutputLogTab
{
	OutputLogTabFull,
	OutputLogTabErrors,
	OutputLogTabWarnings,
	OutputLogTabMaterials,
	OutputLogTabImages,
	OutputLogTabXModels,
	OutputLogTabPlayerCharacters,
	OutputLogTabConvertingMesh,
	OutputLogTabTechsets,
	OutputLogTabShaders
};

static QString LogFilterSettingKey(LogFilterKind Kind)
{
	switch (Kind)
	{
	case LogFilterLeakMod:
		return "LogFilters/ShowLeakMod";
	case LogFilterPhilLibX:
		return "LogFilters/ShowPhilLibX";
	case LogFilterGdtDb:
		return "LogFilters/ShowGdtDb";
	case LogFilterLinking:
		return "LogFilters/ShowLinking";
	case LogFilterCommands:
		return "LogFilters/ShowCommands";
	case LogFilterWarnings:
		return "LogFilters/ShowWarnings";
	case LogFilterErrors:
		return "LogFilters/ShowErrors";
	default:
		return QString();
	}
}

static bool MatchesLogFilterKind(LogFilterKind Kind, const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	switch (Kind)
	{
	case LogFilterLeakMod:
		return Lower.contains("l3akmod") || Lower.contains("leakmod");
	case LogFilterPhilLibX:
		return Lower.contains("phillibx") || Lower.contains("philibx") || Lower.contains("philipx") || Lower.contains("philip") || Lower.contains("t7mtenhancements");
	case LogFilterGdtDb:
		return Lower.startsWith("gdtdb:");
	case LogFilterLinking:
		return Lower.startsWith("linking ") || Lower == "processing..." || Lower.startsWith("done:");
	case LogFilterCommands:
		return DetectLogMessageKind(Text) == LogMessageCommand || DetectLogMessageKind(Text) == LogMessageLaunch;
	case LogFilterWarnings:
		return DetectLogMessageKind(Text) == LogMessageWarning;
	case LogFilterErrors:
		return DetectLogMessageKind(Text) == LogMessageError;
	default:
		return false;
	}
}

static bool IsLogFilterEnabled(const QSettings& Settings, LogFilterKind Kind)
{
	const QString Key = LogFilterSettingKey(Kind);
	return Key.isEmpty() ? true : Settings.value(Key, true).toBool();
}

static bool MatchesOutputLogTab(int TabIndex, const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	switch (TabIndex)
	{
	case OutputLogTabFull:
		return true;
	case OutputLogTabErrors:
		return DetectLogMessageKind(Text) == LogMessageError || Lower.startsWith("error:") || Lower.contains(" failed");
	case OutputLogTabWarnings:
		return DetectLogMessageKind(Text) == LogMessageWarning || Lower.contains("warning");
	case OutputLogTabMaterials:
		return Lower.startsWith("material:") || Lower.startsWith("material_draw_method:");
	case OutputLogTabImages:
		return Lower.startsWith("image:") || Lower.startsWith("images:");
	case OutputLogTabXModels:
		return Lower.startsWith("xmodel:");
	case OutputLogTabPlayerCharacters:
		return Lower.startsWith("playerbodystyle:") || Lower.startsWith("playerbodytype:") || Lower.startsWith("customizationtable:") || Lower.startsWith("character:");
	case OutputLogTabConvertingMesh:
		return Lower.contains("converting") || Lower.contains("mesh") || Lower.contains("xmodel_export") || Lower.contains("xanim_export");
	case OutputLogTabTechsets:
		return Lower.startsWith("techset:");
	case OutputLogTabShaders:
		return Lower.contains("shader") || Lower.contains("technique(") || Lower.contains("gettechnique(");
	default:
		return true;
	}
}

static bool ShouldDisplayLogOutput(const QSettings& Settings, const QString& Output, int OutputTabIndex = OutputLogTabFull)
{
	if (!MatchesOutputLogTab(OutputTabIndex, Output))
		return false;

	const LogFilterKind FilterOrder[] =
	{
		LogFilterLeakMod,
		LogFilterPhilLibX,
		LogFilterGdtDb,
		LogFilterLinking,
		LogFilterCommands,
		LogFilterWarnings,
		LogFilterErrors
	};

	for (int FilterIdx = 0; FilterIdx < static_cast<int>(sizeof(FilterOrder) / sizeof(FilterOrder[0])); FilterIdx++)
	{
		const LogFilterKind Kind = FilterOrder[FilterIdx];
		if (MatchesLogFilterKind(Kind, Output))
			return IsLogFilterEnabled(Settings, Kind);
	}

	return true;
}

static bool IsLogBlockHeader(const QString& Text)
{
	const QString Lower = Text.trimmed().toLower();
	if (Lower.isEmpty())
		return false;

	return Lower.startsWith("gdtdb:")
		|| Lower.startsWith("[l3akmod")
		|| Lower.startsWith("phillibx")
		|| Lower.startsWith("philibx")
		|| Lower.startsWith("philipx")
		|| Lower.startsWith("philip")
		|| Lower.startsWith("linking ")
		|| Lower.startsWith("^1material ")
		|| Lower.startsWith("<error:")
		|| Lower.contains("linker_modtools.exe")
		|| Lower.contains("cod2map64.exe")
		|| Lower.contains("radiant_modtools.exe")
		|| Lower.contains("blackops3.exe")
		|| Lower.contains(": error x")
		|| Lower.contains(": warning x");
}

static bool IsUnrecoverableLogLine(const QString& Text)
{
	const QString Trimmed = Text.trimmed();
	const QString Lower = Trimmed.toLower();
	if (Trimmed.isEmpty())
		return false;

	bool AllStars = true;
	for (int CharIdx = 0; CharIdx < Trimmed.length(); CharIdx++)
	{
		if (Trimmed[CharIdx] != '*')
		{
			AllStars = false;
			break;
		}
	}

	return AllStars
		|| Lower == "unrecoverable error:"
		|| Lower.contains("one or more shaders failed to compile")
		|| Lower == "linker will now terminate.";
}

static bool ShouldContinueCurrentLogBlock(const QString& CurrentBlockText, const QString& NextLine)
{
	const QString CurrentLower = CurrentBlockText.toLower();
	const QString NextLower = NextLine.trimmed().toLower();
	if (CurrentLower.isEmpty() || NextLower.isEmpty())
		return false;

	const bool GdtDbBlock = CurrentLower.contains("gdtdb.exe") || CurrentLower.contains("gdtdb:");
	if (GdtDbBlock)
	{
		return NextLower.startsWith("gdtdb:")
			|| NextLower.startsWith("processed ")
			|| NextLower.startsWith("processed(")
			|| NextLower.startsWith("gdtdb: successfully updated database");
	}

	return false;
}

static int LogDetailIndentLevel(const QString& Text);

static QString RenderedLogLine(const QString& RawLine)
{
	const QString TrimmedLine = RawLine.trimmed();
	if (TrimmedLine == RawLine && LogDetailIndentLevel(RawLine) > 0)
		return QString(LogDetailIndentLevel(RawLine) * 4, ' ') + TrimmedLine;
	return RawLine;
}

static QStringList ExtractLogBlocks(const QString& Output)
{
	QStringList Blocks;
	QString CurrentBlockText;
	const QString NormalizedOutput = QString(Output).replace("\r", "");
	const QStringList OutputLines = NormalizedOutput.split('\n');

	for (const QString& RawLine : OutputLines)
	{
		const QString TrimmedLine = RawLine.trimmed();
		if (TrimmedLine.isEmpty())
			continue;

		const bool CurrentBlockIsUnrecoverable = !CurrentBlockText.isEmpty() && IsUnrecoverableLogLine(CurrentBlockText);
		const bool ContinueUnrecoverableBlock = CurrentBlockIsUnrecoverable && IsUnrecoverableLogLine(TrimmedLine);
		const bool ContinueCurrentBlock = !CurrentBlockText.isEmpty() && ShouldContinueCurrentLogBlock(CurrentBlockText, TrimmedLine);
		const bool StartsNewBlock = !ContinueUnrecoverableBlock && !ContinueCurrentBlock && (CurrentBlockText.isEmpty() || IsLogBlockHeader(TrimmedLine) || LogDetailIndentLevel(RawLine) <= 0);

		if (StartsNewBlock)
		{
			if (!CurrentBlockText.isEmpty())
				Blocks.append(CurrentBlockText);
			CurrentBlockText = TrimmedLine;
			continue;
		}

		if (!CurrentBlockText.isEmpty())
			CurrentBlockText += "\n";
		CurrentBlockText += RenderedLogLine(RawLine);
	}

	if (!CurrentBlockText.isEmpty())
		Blocks.append(CurrentBlockText);

	return Blocks;
}

static bool ShouldDisplayLogBlock(const QSettings& Settings, const QString& BlockText, int OutputTabIndex)
{
	const QStringList Lines = BlockText.split('\n');
	for (const QString& Line : Lines)
	{
		const QString TrimmedLine = Line.trimmed();
		if (TrimmedLine.isEmpty())
			continue;
		if (ShouldDisplayLogOutput(Settings, TrimmedLine, OutputTabIndex))
			return true;
	}

	return false;
}

static bool IsScrollBarAtBottom(const QScrollBar* ScrollBar)
{
	return !ScrollBar || ScrollBar->value() >= ScrollBar->maximum();
}

static int LogDetailIndentLevel(const QString& Text)
{
	int LeadingSpaces = 0;
	while (LeadingSpaces < Text.length() && (Text[LeadingSpaces] == ' ' || Text[LeadingSpaces] == '\t'))
		LeadingSpaces++;

	if (LeadingSpaces > 0)
		return qMax(1, LeadingSpaces / 2);

	const QString Lower = Text.trimmed().toLower();
	if (Lower == "processing..." || Lower.startsWith("done:"))
		return 1;
	if (Lower.startsWith("material_draw_method") || Lower.startsWith("technique[") || Lower.startsWith("technique(") || Lower.startsWith("<error:"))
		return 1;
	if (Lower.startsWith("techset:") || Lower.startsWith("material:") || Lower.startsWith("csv:") || Lower.startsWith("weapon:") || Lower.startsWith("weaponcamo:"))
		return 2;
	return 0;
}

static QColor OutputBlockBackgroundColor(int ThemeModeValue, LogMessageKind Kind, int BlockIndex)
{
	const bool UseClassicChrome = ThemeUsesClassicChrome(ThemeModeValue);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(ThemeModeValue);
	Q_UNUSED(BlockIndex);
	QColor BaseColor;
	if (UseClassicChrome)
		BaseColor = QApplication::palette().color(QPalette::Base);
	else if (UseDarkModernChrome)
		BaseColor = QColor(8, 10, 13, (BlockIndex % 2) ? 108 : 120);
	else
		BaseColor = QColor(16, 16, 16, (BlockIndex % 2) ? 96 : 112);

	if (Kind == LogMessageWarning)
		BaseColor = UseClassicChrome ? QColor(90, 90, 90, 255) : (UseDarkModernChrome ? QColor(40, 28, 12, 128) : QColor(44, 32, 16, 122));
	else if (Kind == LogMessageError)
		BaseColor = UseClassicChrome ? QColor(90, 90, 90, 255) : (UseDarkModernChrome ? QColor(40, 16, 16, 132) : QColor(42, 18, 18, 126));
	else if (Kind == LogMessageSuccess)
		BaseColor = UseClassicChrome ? QColor(90, 90, 90, 255) : (UseDarkModernChrome ? QColor(16, 34, 24, 122) : QColor(20, 36, 26, 116));
	else if (Kind == LogMessageAccent || Kind == LogMessageLaunch)
		BaseColor = UseClassicChrome ? QColor(90, 90, 90, 255) : (UseDarkModernChrome ? QColor(28, 20, 38, 124) : QColor(32, 24, 42, 118));

	return BaseColor;
}

static QString LogBlockText(const QTreeWidgetItem* Item)
{
	if (!Item)
		return QString();

	return Item->data(0, ML_LOG_TEXT_ROLE).toString();
}

static void ApplyLogRowVisualStyle(QWidget* RowWidget, const QColor& BackgroundColor, bool RoundTop, bool RoundBottom)
{
	if (!RowWidget)
		return;

	const int TopRadius = RoundTop ? 4 : 0;
	const int BottomRadius = RoundBottom ? 4 : 0;
	RowWidget->setStyleSheet(
		QString("background:%1; border-top-left-radius:%2px; border-top-right-radius:%2px; border-bottom-left-radius:%3px; border-bottom-right-radius:%3px;")
			.arg(BackgroundColor.name(QColor::HexArgb))
			.arg(TopRadius)
			.arg(BottomRadius));
}

static QWidget* CreateLogRowWidget(QTreeWidget* Parent, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, int IndentLevel, bool IsHeader, bool RoundTop = true, bool RoundBottom = true)
{
	QWidget* RowWidget = new QWidget(Parent);
	RowWidget->setObjectName(IsHeader ? "LogHeaderRow" : "LogDetailRow");
	QHBoxLayout* Layout = new QHBoxLayout(RowWidget);
	Layout->setContentsMargins(8 + (IndentLevel * 16), IsHeader ? 5 : 1, 8, IsHeader ? 5 : 1);
	Layout->setSpacing(0);

	QLabel* Label = new QLabel(Text, RowWidget);
	Label->setTextFormat(Qt::PlainText);
	Label->setWordWrap(false);
	Label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	Label->setCursor(Qt::IBeamCursor);
	Label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	QFont FixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	FixedFont.setPointSize(qMax(8, FixedFont.pointSize()));
	Label->setFont(FixedFont);
	Label->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	Layout->addWidget(Label, 1);

	ApplyLogRowVisualStyle(RowWidget, BackgroundColor, RoundTop, RoundBottom);
	return RowWidget;
}

static QString DisplayedLogBlockText(const QString& StoredText, bool Expanded)
{
	if (Expanded)
		return StoredText;

	const int NewlineIdx = StoredText.indexOf('\n');
	return NewlineIdx >= 0 ? StoredText.left(NewlineIdx) : StoredText;
}

static QFont LogBlockFont()
{
	QFont FixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	FixedFont.setPointSize(qMax(8, FixedFont.pointSize()));
	return FixedFont;
}

static int LogBlockWidgetHeight(const QString& Text, bool Expanded)
{
	const QString DisplayText = DisplayedLogBlockText(Text, Expanded);
	const int LineCount = qMax(1, DisplayText.count('\n') + 1);
	return (QFontMetrics(LogBlockFont()).lineSpacing() * LineCount) + 18;
}

static QString LogBlockLineColor(const QString& Line, const QString& DefaultColor)
{
	const QString TrimmedLower = Line.trimmed().toLower();
	if (TrimmedLower.startsWith("linking ") || TrimmedLower == "processing..." || TrimmedLower.startsWith("done:"))
		return QString("#6ee7a8");
	if (TrimmedLower.startsWith("techset:"))
		return QString("#c792ea");
	if (TrimmedLower.startsWith("material:"))
		return QString("#6cb6ff");
	if (TrimmedLower.startsWith("material_draw_method:"))
		return QString("#f6bd60");
	if (TrimmedLower.startsWith("xmodel:"))
		return QString("#4fd1c5");
	if (TrimmedLower.startsWith("playerbodystyle:"))
		return QString("#7ee787");
	if (TrimmedLower.startsWith("playerbodytype:"))
		return QString("#9be9a8");
	if (TrimmedLower.startsWith("customizationtable:"))
		return QString("#ffd866");
	if (TrimmedLower.startsWith("csv:"))
		return QString("#8b949e");
	if (TrimmedLower.startsWith("error:") || TrimmedLower.contains(" error:") || TrimmedLower.contains(" failed"))
		return QString("#ff7a7a");
	return DefaultColor;
}

static QString LogBlockHtml(const QString& Text, const QColor& TextColor, bool Expanded)
{
	const QString DisplayText = DisplayedLogBlockText(Text, Expanded);
	const QString DefaultColor = TextColor.name(QColor::HexRgb);
	const QString ErrorColor = QString("#ff7a7a");
	QStringList HtmlLines;
	const QStringList Lines = DisplayText.split('\n');

	for (QString Line : Lines)
	{
		Line.replace(QRegularExpression("\\^1(?=\\s*\\d+:|ERROR\\b)"), QString());
		const QString LineColor = LogBlockLineColor(Line, DefaultColor);

		QString EscapedLine = Line.toHtmlEscaped();
		EscapedLine.replace(" ", "&nbsp;");
		EscapedLine.replace("\t", "&nbsp;&nbsp;&nbsp;&nbsp;");
		EscapedLine.replace(QRegularExpression("ERROR"), QString("<span style=\"color:%1; font-weight:700;\">ERROR</span>").arg(ErrorColor));
		HtmlLines.append(QString("<span style=\"color:%1;\">%2</span>").arg(LineColor, EscapedLine));
	}

	return QString("<div style=\"font-family:'%1'; font-size:%2pt; white-space:nowrap;\">%3</div>")
		.arg(LogBlockFont().family())
		.arg(LogBlockFont().pointSize())
		.arg(HtmlLines.join("<br/>"));
}

static QWidget* CreateLogBlockWidget(QWidget* Parent, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, bool Expanded)
{
	QWidget* BlockWidget = new QWidget(Parent);
	BlockWidget->setObjectName("LogBlockWidget");
	BlockWidget->setFocusPolicy(Qt::ClickFocus);
	QVBoxLayout* Layout = new QVBoxLayout(BlockWidget);
	Layout->setContentsMargins(8, 5, 8, 5);
	Layout->setSpacing(0);

	QLabel* BlockLabel = new QLabel(BlockWidget);
	BlockLabel->setObjectName("LogBlockText");
	BlockLabel->setFocusPolicy(Qt::ClickFocus);
	BlockLabel->setTextFormat(Qt::RichText);
	BlockLabel->setWordWrap(false);
	BlockLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	BlockLabel->setCursor(Qt::IBeamCursor);
	BlockLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	BlockLabel->setFont(LogBlockFont());
	BlockLabel->setText(LogBlockHtml(Text, TextColor, Expanded));
	BlockLabel->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	BlockLabel->setFixedHeight(LogBlockWidgetHeight(Text, Expanded) - 10);
	Layout->addWidget(BlockLabel);
	BlockWidget->setFixedHeight(LogBlockWidgetHeight(Text, Expanded));

	ApplyLogRowVisualStyle(BlockWidget, BackgroundColor, true, true);
	return BlockWidget;
}

static void UpdateLogBlockWidget(QWidget* BlockWidget, const QString& Text, const QColor& TextColor, const QColor& BackgroundColor, bool Expanded)
{
	if (!BlockWidget)
		return;

	QLabel* BlockLabel = BlockWidget->findChild<QLabel*>("LogBlockText");
	if (!BlockLabel)
		return;

	BlockLabel->setText(LogBlockHtml(Text, TextColor, Expanded));
	BlockLabel->setFixedHeight(LogBlockWidgetHeight(Text, Expanded) - 10);
	BlockLabel->setStyleSheet(QString("background: transparent; color: %1;").arg(TextColor.name(QColor::HexRgb)));
	BlockWidget->setFixedHeight(LogBlockWidgetHeight(Text, Expanded));
	ApplyLogRowVisualStyle(BlockWidget, BackgroundColor, true, true);
}

static bool UseImprovedConsoleStyle(const QSettings& Settings)
{
	return Settings.value("ConsoleStyle", "improved").toString().compare("original", Qt::CaseInsensitive) != 0;
}

static LogMessageKind DetectLogMessageKind(const QString& Output)
{
	const QString Text = Output.trimmed();
	const QString Lower = Text.toLower();

	if (Lower.isEmpty())
		return LogMessageDefault;

	if (Lower.contains("leakmod"))
		return LogMessageMuted;

	if (Lower.contains("phillibx") || Lower.contains("philibx") || Lower.contains("philipx") || Lower.contains("philip") || Lower.contains("t7mtenhancements"))
		return LogMessageAccent;

	if (Lower.startsWith("linking ") && Lower.contains("done:") && !Lower.contains("warning") && !Lower.contains("error") && !Lower.contains("failed"))
		return LogMessageSuccess;

	if (Lower.startsWith("linking "))
		return LogMessageWarning;

	if (Lower.startsWith("done:"))
		return LogMessageSuccess;

	if (Text.startsWith("^1") || Lower.contains("mismatched usage") || Lower.contains("doesn't expose") || Lower.contains("material "))
		return LogMessageWarning;

	if (Lower.contains("blackops3.exe") || Lower.contains("+devmap"))
		return LogMessageLaunch;

	if (Lower.contains(".exe ") || Lower.endsWith(".exe") || Lower.contains("linker_modtools") || Lower.contains("cod2map64") || Lower.contains("radiant_modtools"))
		return LogMessageCommand;

	if (Lower.contains("error") || Lower.contains("failed") || Lower.contains("could not") || Lower.contains("abnormally") || Lower.contains("invalid"))
		return LogMessageError;

	if (Lower.contains("warning") || Lower.contains("skipping") || Lower.contains("english-only") || Lower.contains("english only"))
		return LogMessageWarning;

	if (Lower.contains("success") || Lower.contains("saved") || Lower.contains("created") || Lower.contains("complete") || Lower.contains("updated"))
		return LogMessageSuccess;

	if (Lower.contains("uploading") || Lower.contains("preparing") || Lower.contains("committing") || Lower.contains("converting"))
		return LogMessageInfo;

	return LogMessageDefault;
}

static QColor ColorForLogMessageKind(const QSettings& Settings, LogMessageKind Kind)
{
	switch (Kind)
	{
	case LogMessageMuted:
		return QColor(Settings.value("LogColors/Muted", "#8b929a").toString());
	case LogMessageCommand:
		return QColor(Settings.value("LogColors/Command", "#7dcfff").toString());
	case LogMessageInfo:
		return QColor(Settings.value("LogColors/Info", "#eef1f4").toString());
	case LogMessageSuccess:
		return QColor(Settings.value("LogColors/Success", "#6ee7a8").toString());
	case LogMessageWarning:
		return QColor(Settings.value("LogColors/Warning", "#ffcf70").toString());
	case LogMessageError:
		return QColor(Settings.value("LogColors/Error", "#ff7a7a").toString());
	case LogMessageLaunch:
		return QColor(Settings.value("LogColors/Launch", "#c792ea").toString());
	case LogMessageAccent:
		return QColor(Settings.value("LogColors/Accent", "#c792ea").toString());
	default:
		return QColor(Settings.value("LogColors/Default", "#d7dce2").toString());
	}
}

static QString PrepareBackgroundImageCache(const QString& CacheRoot, const QString& SourcePath, int OpacityPercent)
{
	const QString NormalizedPath = SourcePath.trimmed();
	if (NormalizedPath.isEmpty())
		return QString();

	const bool IsQtResource = NormalizedPath.startsWith(":/") || NormalizedPath.startsWith("qrc:/");
	if (!IsQtResource && !QFileInfo(NormalizedPath).isFile())
		return QString();

	const QFileInfo Info(NormalizedPath);
	const QString CacheSourceKey = IsQtResource
		? NormalizedPath
		: QString("%1|%2|%3")
			.arg(QDir::toNativeSeparators(Info.absoluteFilePath()))
			.arg(Info.lastModified().toMSecsSinceEpoch())
			.arg(Info.size());
	const QByteArray CacheKey = QCryptographicHash::hash(
		QString("%1|%2").arg(CacheSourceKey).arg(qBound(0, OpacityPercent, 100)).toUtf8(),
		QCryptographicHash::Md5).toHex();
	const QString CacheDir = QDir::cleanPath(CacheRoot + "/background_cache");
	const QString CachePath = QDir::cleanPath(QString("%1/%2.png").arg(CacheDir, QString(CacheKey)));
	if (QFileInfo(CachePath).isFile())
		return CachePath;

	QPixmap SourcePixmap(NormalizedPath);
	if (SourcePixmap.isNull())
		return QString();

	QDir().mkpath(CacheDir);
	QPixmap OutputPixmap(SourcePixmap.size());
	OutputPixmap.fill(Qt::transparent);
	QPainter Painter(&OutputPixmap);
	Painter.setOpacity(qBound(0, OpacityPercent, 100) / 100.0);
	Painter.drawPixmap(0, 0, SourcePixmap);
	Painter.end();
	OutputPixmap.save(CachePath, "PNG");
	return CachePath;
}

static BOOL CALLBACK FindVisibleWindowForProcess(HWND WindowHandle, LPARAM Param)
{
	GameWindowSearchState* State = reinterpret_cast<GameWindowSearchState*>(Param);
	DWORD WindowProcessId = 0;
	GetWindowThreadProcessId(WindowHandle, &WindowProcessId);
	if (WindowProcessId != State->ProcessId || !IsWindowVisible(WindowHandle) || GetWindow(WindowHandle, GW_OWNER) != NULL)
		return TRUE;

	State->HasVisibleWindow = true;
	return FALSE;
}

mlBuildThread::mlBuildThread(const QList<QPair<QString, QStringList>>& Commands, bool IgnoreErrors)
	: mCommands(Commands), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors), mActiveProcessId(0)
{
}

void mlBuildThread::run()
{
	bool Success = true;

	for (const QPair<QString, QStringList>& Command : mCommands)
	{
		if (IsGameLaunchCommand(Command.first, Command.second))
		{
			emit OutputReady(Command.first + ' ' + Command.second.join(' ') + "\n");
			QProcess::startDetached(Command.first, Command.second, QFileInfo(Command.first).absolutePath());
			continue;
		}

		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(QFileInfo(Command.first).absolutePath());
		Process->setProcessChannelMode(QProcess::MergedChannels);

		emit OutputReady(Command.first + ' ' + Command.second.join(' ') + "\n");

		Process->start(Command.first, Command.second);
		if (!Process->waitForStarted(5000))
			return;
		mActiveProcessId = Process->processId();
		for (;;)
		{
			Sleep(100);

			if (Process->waitForReadyRead(0))
				emit OutputReady(Process->readAll());

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}
		mActiveProcessId = 0;

		if (Process->exitStatus() != QProcess::NormalExit)
			return;

		if (Process->exitCode() != 0)
		{
			Success = false;
			if (!mIgnoreErrors)
				return;
		}
	}

	mActiveProcessId = 0;
	mSuccess = Success;
}

mlConvertThread::mlConvertThread(QStringList& Files, QString& OutputDir, bool IgnoreErrors, bool Overwrite)
	: mFiles(Files), mOutputDir(OutputDir), mOverwrite(Overwrite), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors), mActiveProcessId(0)
{
}

void mlConvertThread::run()
{
	bool Success = true;
	unsigned int convCountSuccess = 0;
	unsigned int convCountSkipped = 0;
	unsigned int convCountFailed = 0;

	for (QString file : mFiles)
	{
		QFileInfo file_info(file);
		QString working_directory = file_info.absolutePath();

		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(working_directory);
		Process->setProcessChannelMode(QProcess::MergedChannels);

		file = file_info.baseName();

		QString ToolsPath = QDir::fromNativeSeparators(getenv("TA_TOOLS_PATH"));
		QString ExecutablePath = QString("%1bin/export2bin.exe").arg(ToolsPath);

		QStringList args;
		args.append("/piped");

		QString filepath = file_info.absoluteFilePath();

		QString ext = file_info.suffix().toUpper();
		if (ext == "XANIM_EXPORT")
			ext = ".XANIM_BIN";
		else if (ext == "XMODEL_EXPORT")
			ext = ".XMODEL_BIN";
		else
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file has invalid extension)\n");
			convCountSkipped++;
			continue;
		}

		QString target_filepath = QDir::cleanPath(mOutputDir) + QDir::separator() + file + ext;

		QFile infile(filepath);
		QFile outfile(target_filepath);

		if (!mOverwrite && outfile.exists())
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file already exists)\n");
			convCountSkipped++;
			continue;
		}

		infile.open(QIODevice::OpenMode::enum_type::ReadOnly);
		if (!infile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + filepath + "' for reading\n");
			convCountFailed++;
			continue;
		}

		emit OutputReady("Export2Bin: Converting '" + file + "'");

		QByteArray buf = infile.readAll();
		infile.close();

		Process->start(ExecutablePath, args);
		if (!Process->waitForStarted(5000))
		{
			convCountFailed++;
			continue;
		}
		mActiveProcessId = Process->processId();
		Process->write(buf);
		Process->closeWriteChannel();

		QByteArray standardOutputPipeData;
		QByteArray standardErrorPipeData;

		for (;;)
		{
			Sleep(20);
			if (Process->waitForReadyRead(0))
			{
				standardOutputPipeData.append(Process->readAllStandardOutput());
				standardErrorPipeData.append(Process->readAllStandardError());
			}

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}
		mActiveProcessId = 0;

		if (Process->exitStatus() != QProcess::NormalExit)
		{
			emit OutputReady("ERROR: Process exited abnormally");
			Success = false;
			break;
		}

		if (Process->exitCode() != 0)
		{
			emit OutputReady(standardOutputPipeData);
			emit OutputReady(standardErrorPipeData);
			convCountFailed++;
			if (!mIgnoreErrors)
			{
				Success = false;
				break;
			}
			continue;
		}

		outfile.open(QIODevice::OpenMode::enum_type::WriteOnly);
		if (!outfile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + target_filepath + "' for writing\n");
			continue;
		}

		outfile.write(standardOutputPipeData);
		outfile.close();
		convCountSuccess++;
	}

	mActiveProcessId = 0;
	mSuccess = Success;
	if (mSuccess)
	{
		QString msg = QString("Export2Bin: Finished!\n\n"
			"Files Processed: %1\n"
			"Successes: %2\n"
			"Skipped: %3\n"
			"Failures: %4\n").arg(mFiles.count()).arg(convCountSuccess).arg(convCountSkipped).arg(convCountFailed);
		emit OutputReady(msg);
	}
}

mlMainWindow::mlMainWindow()
{
	QSettings Settings;
	EnsureThemeProfiles();

	mBuildThread = NULL;
	mConvertThread = NULL;
	mActiveBuildButton = NULL;
	mThemesButton = NULL;
	mActionThemeModern = NULL;
	mActionThemeDarkModern = NULL;
	mActionThemeClassic = NULL;
	mCategoryTabs = NULL;
	mOutputTabs = NULL;
	mCentralWidgetSplitter = NULL;
	mTopWidget = NULL;
	mLeftPanel = NULL;
	mActionsPanel = NULL;
	mOutputPanel = NULL;
	mTopLayout = NULL;
	mFileListWidget = NULL;
	mOutputWidget = NULL;
	mOutputPlainWidget = NULL;
	mAssetTreeBackgroundOverlay = NULL;
	mOutputBackgroundOverlay = NULL;
	mLogFiltersButton = NULL;
	mLogSelectionButton = NULL;
	mRunOnlineWidget = NULL;
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mCloseGameButton = NULL;
	mCloseGameStatusLabel = NULL;
	mLaunchProgressDialog = NULL;
	mCachedGameRunning = false;
	mGameProcessId = 0;
	mGameRunningState = GameNotRunning;
	mBuildLanguage = Settings.value("BuildLanguage", "english").toString();
	mLauncherLayout = Settings.value("LauncherLayout", "original").toString().trimmed().toLower();
	if (mLauncherLayout.isEmpty() || mLauncherLayout == "current")
		mLauncherLayout = "original";
	mThemeMode = ThemeModeFromSettings(Settings);
	mThemeProfileId = CurrentThemeProfileId();
	mLogBackgroundCachePath.clear();
	mAssetTreeBackgroundCachePath.clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mOutputSelectionMode = false;
	mOutputTabIndex = OutputLogTabFull;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	mPendingOnlineLaunchFeedback = false;
	mPendingOnlineLaunchLabel.clear();
	mPostUploadSteamSyncPending = false;
	mOutputFlushTimer.setSingleShot(true);
	connect(&mOutputFlushTimer, &QTimer::timeout, this, &mlMainWindow::FlushBuildOutput);

	// Qt prefers '/' over '\\'
	mGamePath = QString(getenv("TA_GAME_PATH")).replace('\\', '/');
	mToolsPath = QString(getenv("TA_TOOLS_PATH")).replace('\\', '/');

	ApplyThemeProfile(mThemeProfileId);

	setWindowIcon(QIcon(":/resources/ModLauncher.png"));
	setWindowTitle("BO3 Mod Tools Black");
	
	resize(1180, 780);

	CreateActions();
	CreateMenu();
	CreateToolBar();

	mExport2BinGUIWidget = NULL;

	mCentralWidgetSplitter = new QSplitter();
	mCentralWidgetSplitter->setOrientation(Qt::Vertical);

	mTopWidget = new QWidget();

	mTopLayout = new QHBoxLayout(mTopWidget);
	mTopWidget->setLayout(mTopLayout);
	mTopLayout->setContentsMargins(8, 8, 8, 8);
	mTopLayout->setSpacing(10);

	mLeftPanel = new QWidget(mTopWidget);
	mLeftPanel->setObjectName("AssetListPanel");
	QVBoxLayout* LeftLayout = new QVBoxLayout(mLeftPanel);
	LeftLayout->setContentsMargins(0, 0, 0, 0);
	LeftLayout->setSpacing(0);

	mCategoryTabs = new QTabBar(mLeftPanel);
	mCategoryTabs->setObjectName("CategoryTabs");
	mCategoryTabs->addTab("Recent");
	mCategoryTabs->addTab("Favorites");
	mCategoryTabs->addTab("ZM Maps");
	mCategoryTabs->addTab("MP Maps");
	mCategoryTabs->addTab("Mods");
	mCategoryTabs->addTab("All");
	mCategoryTabs->setDocumentMode(true);
	mCategoryTabs->setExpanding(false);
	mCategoryTabs->setDrawBase(false);
	mCategoryTabs->setUsesScrollButtons(false);
	mCategoryTabs->setElideMode(Qt::ElideNone);
	const int SavedCategoryTab = Settings.value("ActiveCategoryTab", CategoryAll).toInt();
	mCategoryTabs->setCurrentIndex(qBound(0, SavedCategoryTab, mCategoryTabs->count() - 1));
	LeftLayout->addWidget(mCategoryTabs);

	mFileListWidget = new QTreeWidget();
	mFileListWidget->setObjectName("AssetTree");
	mFileListWidget->setColumnCount(1);
	mFileListWidget->setHeaderHidden(true);
	mFileListWidget->setUniformRowHeights(false);
	mFileListWidget->setRootIsDecorated(true);
	mFileListWidget->setAlternatingRowColors(false);
	mFileListWidget->setIndentation(18);
	mFileListWidget->header()->setStretchLastSection(false);
	mFileListWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	mFileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	mFileListWidget->setContentsMargins(0, 0, 0, 0);
	mFileListWidget->viewport()->setObjectName("AssetTreeViewport");
	mFileListWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mFileListWidget->viewport()->installEventFilter(this);
	mAssetTreeBackgroundOverlay = new QLabel(mFileListWidget->viewport());
	mAssetTreeBackgroundOverlay->setObjectName("AssetTreeBackgroundOverlay");
	mAssetTreeBackgroundOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mAssetTreeBackgroundOverlay->setScaledContents(false);
	mAssetTreeBackgroundOverlay->setAlignment(Qt::AlignCenter);
	mAssetTreeBackgroundOverlay->hide();
	mAssetTreeBackgroundOverlay->lower();
	LeftLayout->addWidget(mFileListWidget);
	mTopLayout->addWidget(mLeftPanel);
	mTopLayout->setStretch(0, 1);

	connect(mCategoryTabs, &QTabBar::currentChanged, this, [=](int Index)
	{
		QSettings().setValue("ActiveCategoryTab", Index);
		PopulateFileList();
	});

	connect(mFileListWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(ContextMenuRequested()));
	connect(mFileListWidget, &QTreeWidget::itemExpanded, this, [=](QTreeWidgetItem* Item)
	{
		if (!Item || Item->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			return;
		QSettings().setValue(SectionSettingKey(Item->text(0)) + "/Expanded", true);
	});
	connect(mFileListWidget, &QTreeWidget::itemCollapsed, this, [=](QTreeWidgetItem* Item)
	{
		if (!Item || Item->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			return;
		QSettings().setValue(SectionSettingKey(Item->text(0)) + "/Expanded", false);
	});
	connect(mFileListWidget, &QTreeWidget::currentItemChanged, this, [=](QTreeWidgetItem* CurrentItem, QTreeWidgetItem* PreviousItem)
	{
		SyncItemSelectionWidget(PreviousItem);
		SyncItemSelectionWidget(CurrentItem);
		UpdateGameRunningState();
	});

	mActionsPanel = new QWidget(mTopWidget);
	mActionsPanel->setObjectName("ActionsPanel");
	mActionsPanel->setMinimumWidth(220);
	mActionsPanel->setMaximumWidth(220);
	mActionsPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	mTopLayout->addWidget(mActionsPanel);

	QVBoxLayout* ActionsLayout = new QVBoxLayout(mActionsPanel);
	ActionsLayout->setContentsMargins(10, 10, 10, 10);
	ActionsLayout->setSpacing(10);

	QLabel* BuildOptionsLabel = new QLabel("Build Options");
	ActionsLayout->addWidget(BuildOptionsLabel);

	QHBoxLayout* CompileLayout = new QHBoxLayout();
	ActionsLayout->addLayout(CompileLayout);

	mCompileEnabledWidget = new QCheckBox("Compile");
	CompileLayout->addWidget(mCompileEnabledWidget);

	mCompileModeWidget = new QComboBox();
	mCompileModeWidget->addItems(QStringList() << "Ents" << "Full");
	mCompileModeWidget->setCurrentIndex(1);
	CompileLayout->addWidget(mCompileModeWidget);

	QHBoxLayout* LightLayout = new QHBoxLayout();
	ActionsLayout->addLayout(LightLayout);

	mLightEnabledWidget = new QCheckBox("Light");
	LightLayout->addWidget(mLightEnabledWidget);

	mLightQualityWidget = new QComboBox();
	mLightQualityWidget->addItems(QStringList() << "Low" << "Medium" << "High");
	mLightQualityWidget->setCurrentIndex(1);
	mLightQualityWidget->setMinimumWidth(64); // Fix for "Medium" being cut off in the dark theme
	LightLayout->addWidget(mLightQualityWidget);

	mLinkEnabledWidget = new QCheckBox("Link");
	ActionsLayout->addWidget(mLinkEnabledWidget);

	mRunEnabledWidget = new QCheckBox("Run");
	ActionsLayout->addWidget(mRunEnabledWidget);

	mRunOptionsWidget = new QLineEdit();
	mRunOptionsWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	mRunOptionsWidget->setPlaceholderText("Extra args...");
	ActionsLayout->addWidget(mRunOptionsWidget);
	connect(mCompileEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mLightEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mLinkEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	connect(mRunEnabledWidget, &QCheckBox::toggled, this, [=](bool) { UpdateBuildActionButtons(); });
	QLabel* QuickLaunchLabel = new QLabel("Quick Launch Map");
	ActionsLayout->addWidget(QuickLaunchLabel);

	mQuickLaunchWidget = new QComboBox();
	mQuickLaunchWidget->setObjectName("QuickLaunchCombo");
	mQuickLaunchWidget->setMinimumWidth(206);
	mQuickLaunchWidget->setMinimumHeight(28);
	mQuickLaunchWidget->setMaximumHeight(28);
	mQuickLaunchWidget->addItem("No Map", "");
	mQuickLaunchWidget->addItem("Zombies: Shadows of Evil (zm_zod)", "zm_zod");
	mQuickLaunchWidget->addItem("Zombies: The Giant (zm_factory)", "zm_factory");
	mQuickLaunchWidget->addItem("Zombies: Der Eisendrache (zm_castle)", "zm_castle");
	mQuickLaunchWidget->addItem("Zombies: Zetsubou No Shima (zm_island)", "zm_island");
	mQuickLaunchWidget->addItem("Zombies: Gorod Krovi (zm_stalingrad)", "zm_stalingrad");
	mQuickLaunchWidget->addItem("Zombies: Revelations (zm_genesis)", "zm_genesis");
	mQuickLaunchWidget->addItem("Zombies: Nacht der Untoten (zm_prototype)", "zm_prototype");
	mQuickLaunchWidget->addItem("Zombies: Verruckt (zm_asylum)", "zm_asylum");
	mQuickLaunchWidget->addItem("Zombies: Shi No Numa (zm_sumpf)", "zm_sumpf");
	mQuickLaunchWidget->addItem("Zombies: Kino der Toten (zm_theater)", "zm_theater");
	mQuickLaunchWidget->addItem("Zombies: Ascension (zm_cosmodrome)", "zm_cosmodrome");
	mQuickLaunchWidget->addItem("Zombies: Shangri-La (zm_temple)", "zm_temple");
	mQuickLaunchWidget->addItem("Zombies: Moon (zm_moon)", "zm_moon");
	mQuickLaunchWidget->addItem("Zombies: Origins (zm_tomb)", "zm_tomb");
	mQuickLaunchWidget->addItem("Zombies: Dead Ops Arcade 2 (cp_doa_bo3)", "cp_doa_bo3");
	mQuickLaunchWidget->insertSeparator(mQuickLaunchWidget->count());
	mQuickLaunchWidget->addItem("MP: Aquarium (mp_biodome)", "mp_biodome");
	mQuickLaunchWidget->addItem("MP: Breach (mp_spire)", "mp_spire");
	mQuickLaunchWidget->addItem("MP: Combine (mp_sector)", "mp_sector");
	mQuickLaunchWidget->addItem("MP: Evac (mp_apartments)", "mp_apartments");
	mQuickLaunchWidget->addItem("MP: Exodus (mp_chinatown)", "mp_chinatown");
	mQuickLaunchWidget->addItem("MP: Fringe (mp_veiled)", "mp_veiled");
	mQuickLaunchWidget->addItem("MP: Havoc (mp_havoc)", "mp_havoc");
	mQuickLaunchWidget->addItem("MP: Hunted (mp_ethiopia)", "mp_ethiopia");
	mQuickLaunchWidget->addItem("MP: Infection (mp_infection)", "mp_infection");
	mQuickLaunchWidget->addItem("MP: Metro (mp_metro)", "mp_metro");
	mQuickLaunchWidget->addItem("MP: Redwood (mp_redwood)", "mp_redwood");
	mQuickLaunchWidget->addItem("MP: Stronghold (mp_stonghold)", "mp_stonghold");
	mQuickLaunchWidget->addItem("MP: Nuk3town (mp_nuketown_x)", "mp_nuketown_x");
	mQuickLaunchWidget->addItem("MP: Rise (mp_rise)", "mp_rise");
	mQuickLaunchWidget->addItem("MP: Splash (mp_waterpark)", "mp_waterpark");
	mQuickLaunchWidget->addItem("MP: Skyjacked (mp_skyjacked)", "mp_skyjacked");
	mQuickLaunchWidget->addItem("MP: Gauntlet (mp_crucible)", "mp_crucible");
	mQuickLaunchWidget->addItem("MP: Knockout (mp_kung_fu)", "mp_kung_fu");
	mQuickLaunchWidget->addItem("MP: Rift (mp_conduit)", "mp_conduit");
	mQuickLaunchWidget->addItem("MP: Spire (mp_aerospace)", "mp_aerospace");
	mQuickLaunchWidget->addItem("MP: Verge (mp_banzai)", "mp_banzai");
	mQuickLaunchWidget->addItem("MP: Rumble (mp_arena)", "mp_arena");
	mQuickLaunchWidget->addItem("MP: Berserk (mp_shrine)", "mp_shrine");
	mQuickLaunchWidget->addItem("MP: Cryogen (mp_cryogen)", "mp_cryogen");
	mQuickLaunchWidget->addItem("MP: Empire (mp_rome)", "mp_rome");
	mQuickLaunchWidget->addItem("MP: Micro (mp_miniature)", "mp_miniature");
	mQuickLaunchWidget->addItem("MP: Rupture (mp_city)", "mp_city");
	mQuickLaunchWidget->addItem("MP: Outlaw (mp_western)", "mp_western");
	mQuickLaunchWidget->addItem("MP: Citadel (mp_ruins)", "mp_ruins");
	mQuickLaunchWidget->insertSeparator(mQuickLaunchWidget->count());
	mQuickLaunchWidget->addItem("Campaign Safehouse: Mobile (cp_sh_mobile)", "cp_sh_mobile");
	mQuickLaunchWidget->addItem("Campaign Safehouse: Singapore (cp_sh_singapore)", "cp_sh_singapore");
	mQuickLaunchWidget->addItem("Campaign Safehouse: Cairo (cp_sh_cairo)", "cp_sh_cairo");
	mQuickLaunchWidget->insertSeparator(mQuickLaunchWidget->count());
	mQuickLaunchWidget->addItem("Campaign: Black Ops (cp_mi_eth_prologue)", "cp_mi_eth_prologue");
	mQuickLaunchWidget->addItem("Campaign: New World (cp_mi_zurich_newworld)", "cp_mi_zurich_newworld");
	mQuickLaunchWidget->addItem("Campaign: In Darkness (cp_mi_sing_blackstation)", "cp_mi_sing_blackstation");
	mQuickLaunchWidget->addItem("Campaign: Provocation (cp_mi_sing_biodomes)", "cp_mi_sing_biodomes");
	mQuickLaunchWidget->addItem("Campaign: Hypocenter (cp_mi_sing_sgen)", "cp_mi_sing_sgen");
	mQuickLaunchWidget->addItem("Campaign: Vengeance (cp_mi_sing_vengeance)", "cp_mi_sing_vengeance");
	mQuickLaunchWidget->addItem("Campaign: Rise & Fall (cp_mi_cairo_ramses)", "cp_mi_cairo_ramses");
	mQuickLaunchWidget->addItem("Campaign: Demon Within (cp_mi_cairo_infection)", "cp_mi_cairo_infection");
	mQuickLaunchWidget->addItem("Campaign: Sand Castle (cp_mi_cairo_aquifer)", "cp_mi_cairo_aquifer");
	mQuickLaunchWidget->addItem("Campaign: Lotus Towers (cp_mi_cairo_lotus)", "cp_mi_cairo_lotus");
	mQuickLaunchWidget->addItem("Campaign: Life (cp_mi_zurich_coalescence)", "cp_mi_zurich_coalescence");
	mQuickLaunchWidget->insertSeparator(mQuickLaunchWidget->count());
	mQuickLaunchWidget->addItem("Freerun: Alpha (mp_freerun_01)", "mp_freerun_01");
	mQuickLaunchWidget->addItem("Freerun: Sidewinder (mp_freerun_02)", "mp_freerun_02");
	mQuickLaunchWidget->addItem("Freerun: Infected (mp_freerun_03)", "mp_freerun_03");
	mQuickLaunchWidget->addItem("Freerun: Blackout (mp_freerun_04)", "mp_freerun_04");
	for (int QuickLaunchIdx = 0; QuickLaunchIdx < mQuickLaunchWidget->count(); QuickLaunchIdx++)
	{
		const QString MapCode = mQuickLaunchWidget->itemData(QuickLaunchIdx).toString();
		if (MapCode.isEmpty())
			continue;
		QString Text = mQuickLaunchWidget->itemText(QuickLaunchIdx);
		Text.remove(QRegularExpression("^[^:]+:\\s*"));
		Text.remove(QRegularExpression("\\s*\\([^\\)]*\\)$"));
		mQuickLaunchWidget->setItemText(QuickLaunchIdx, Text);
	}
	if (mQuickLaunchWidget->view())
	{
		mQuickLaunchWidget->view()->setMinimumWidth(250);
		mQuickLaunchWidget->view()->setMinimumHeight(320);
	}
	connect(mQuickLaunchWidget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int Index)
	{
		QStringList ExistingArgs = mRunOptionsWidget->text().split(' ', Qt::SkipEmptyParts);
		for (int ArgIdx = ExistingArgs.count() - 1; ArgIdx >= 0; ArgIdx--)
		{
			if (ExistingArgs[ArgIdx] == "+devmap")
			{
				ExistingArgs.removeAt(ArgIdx);
				if (ArgIdx < ExistingArgs.count())
					ExistingArgs.removeAt(ArgIdx);
			}
		}

		const QString MapCode = mQuickLaunchWidget->itemData(Index).toString();
		if (MapCode.isEmpty())
		{
			mRunOptionsWidget->setText(ExistingArgs.join(' '));
			return;
		}

		ExistingArgs << "+devmap" << MapCode;
		mRunOptionsWidget->setText(ExistingArgs.join(' '));
	});
	ActionsLayout->addWidget(mQuickLaunchWidget);

	QLabel* BuildActionsLabel = new QLabel("Build Actions");
	ActionsLayout->addWidget(BuildActionsLabel);

	mBuildButton = new QPushButton("Build");
	mBuildButton->setObjectName("BuildButton");
	connect(mBuildButton, SIGNAL(clicked()), mActionEditBuild, SLOT(trigger()));
	ActionsLayout->addWidget(mBuildButton);

	mBuildAllLanguagesButton = new QPushButton("Build (English)");
	mBuildAllLanguagesButton->setObjectName("BuildEnglishButton");
	connect(mBuildAllLanguagesButton, SIGNAL(clicked()), mActionEditBuildAllLanguages, SLOT(trigger()));
	ActionsLayout->addWidget(mBuildAllLanguagesButton);
	UpdateBuildActionButtons();

	mDvarsButton = new QPushButton("Dvars");
	connect(mDvarsButton, SIGNAL(clicked()), this, SLOT(OnEditDvars()));
	ActionsLayout->addWidget(mDvarsButton);

	mLogButton = new QPushButton("Save Log");
	connect(mLogButton, SIGNAL(clicked()), this, SLOT(OnSaveLog()));
	ActionsLayout->addWidget(mLogButton);

	mIgnoreErrorsWidget = new QCheckBox("Ignore Errors");
	ActionsLayout->addWidget(mIgnoreErrorsWidget);
	const QString RunOnlineToolTip = "Launch through Steam online mode so custom maps and mods can appear online.";
	auto ShowRunOnlineGuide = [this, RunOnlineToolTip]()
	{
		QMessageBox::information(this, "Online Mode Guide",
			RunOnlineToolTip + "\n\n"
			"Use this when you want the game to launch through Steam instead of the direct executable. "
			"That helps custom maps and mods show up correctly in online mode.\n\n"
			"Leave it off if you want the normal direct launch behavior.");
	};
	mRunOnlineWidget = new QCheckBox("Online Mode");
	mRunOnlineWidget->setChecked(false);
	if (Settings.contains("RunOnlineMode"))
		mRunOnlineWidget->setChecked(Settings.value("RunOnlineMode").toBool());
	else
		Settings.setValue("RunOnlineMode", false);
	QWidget* RunOnlineRow = new QWidget();
	QHBoxLayout* RunOnlineLayout = new QHBoxLayout(RunOnlineRow);
	RunOnlineLayout->setContentsMargins(0, 0, 0, 0);
	RunOnlineLayout->setSpacing(4);
	RunOnlineLayout->addWidget(mRunOnlineWidget);
	QToolButton* RunOnlineHelpButton = new QToolButton(RunOnlineRow);
	RunOnlineHelpButton->setText("?");
	RunOnlineHelpButton->setToolTip("Online mode guide");
	RunOnlineHelpButton->setAutoRaise(true);
	RunOnlineHelpButton->setCursor(Qt::WhatsThisCursor);
	RunOnlineHelpButton->setFixedSize(18, 18);
	connect(RunOnlineHelpButton, &QToolButton::clicked, this, ShowRunOnlineGuide);
	RunOnlineLayout->addWidget(RunOnlineHelpButton);
	RunOnlineLayout->addStretch(1);
	ActionsLayout->addWidget(RunOnlineRow);
	connect(mRunOnlineWidget, &QCheckBox::toggled, this, [=](bool Checked) { QSettings().setValue("RunOnlineMode", Checked); });

	ActionsLayout->addStretch(1);

	QLabel* GameControlLabel = new QLabel("Game Control");
	ActionsLayout->addWidget(GameControlLabel);
	mCloseGameButton = new QPushButton("Close Game");
	connect(mCloseGameButton, &QPushButton::clicked, this, [=]()
	{
		if (mGameRunningState != GameNotRunning)
		{
			QProcess::execute("taskkill", QStringList() << "/IM" << "BlackOps3.exe" << "/F");
			UpdateGameRunningState();
			return;
		}

		if (!mFileListWidget || !mFileListWidget->currentItem() || mFileListWidget->currentItem()->data(0, Qt::UserRole).toInt() == ML_ITEM_UNKNOWN)
			return;

		QTreeWidgetItem* Item = mFileListWidget->currentItem();
		const int ItemType = Item->data(0, Qt::UserRole).toInt();
		const QString MapName = (ItemType == ML_ITEM_MAP) ? GetItemEntryName(Item) : QString();
		const QString FsGame = (ItemType == ML_ITEM_MAP) ? GetItemEntryName(Item) : GetItemContainerName(Item);
		TouchRecentEntry(RecentEntryForItem(ItemType, GetItemContainerName(Item), GetItemEntryName(Item)));
		const QPair<QString, QStringList> LaunchCommand = CreateGameLaunchCommand(FsGame, MapName);
		if (mRunOnlineWidget && mRunOnlineWidget->isChecked())
			ShowOnlineLaunchProgressDialog(ItemType == ML_ITEM_MAP ? QString("map '%1'").arg(MapName) : QString("mod '%1'").arg(FsGame));
		QProcess::startDetached(LaunchCommand.first, LaunchCommand.second, QFileInfo(LaunchCommand.first).absolutePath());
		UpdateGameRunningState();
	});
	ActionsLayout->addWidget(mCloseGameButton);
	mCloseGameStatusLabel = new QLabel("Game is not running");
	mCloseGameStatusLabel->setObjectName("GameStateLabel");
	mCloseGameStatusLabel->setWordWrap(true);
	ActionsLayout->addWidget(mCloseGameStatusLabel);

	mOutputWidget = new QTreeWidget(this);
	mOutputWidget->setObjectName("OutputConsole");
	mOutputWidget->setColumnCount(2);
	mOutputWidget->setHeaderHidden(true);
	mOutputWidget->setRootIsDecorated(false);
	mOutputWidget->setIndentation(0);
	mOutputWidget->setUniformRowHeights(false);
	mOutputWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	mOutputWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	mOutputWidget->setFocusPolicy(Qt::StrongFocus);
	mOutputWidget->setAllColumnsShowFocus(false);
	mOutputWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	mOutputWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	mOutputWidget->header()->setStretchLastSection(false);
	mOutputWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	mOutputWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
	mOutputWidget->header()->resizeSection(1, 68);
	mOutputWidget->installEventFilter(this);
	mOutputWidget->viewport()->setObjectName("OutputConsoleViewport");
	mOutputWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mOutputWidget->viewport()->installEventFilter(this);
	connect(mOutputWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [=](int)
	{
		mOutputTreeAutoFollow = IsScrollBarAtBottom(mOutputWidget ? mOutputWidget->verticalScrollBar() : NULL);
	});
	mOutputPlainWidget = new QPlainTextEdit(this);
	mOutputPlainWidget->setObjectName("OutputConsolePlain");
	mOutputPlainWidget->setReadOnly(true);
	mOutputPlainWidget->setUndoRedoEnabled(false);
	mOutputPlainWidget->setLineWrapMode(QPlainTextEdit::NoWrap);
	mOutputPlainWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mOutputPlainWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mOutputPlainWidget->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	mOutputPlainWidget->setFocusPolicy(Qt::StrongFocus);
	mOutputPlainWidget->viewport()->setObjectName("OutputConsolePlainViewport");
	mOutputPlainWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
	mOutputPlainWidget->viewport()->installEventFilter(this);
	connect(mOutputPlainWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, [=](int)
	{
		mOutputPlainAutoFollow = IsScrollBarAtBottom(mOutputPlainWidget ? mOutputPlainWidget->verticalScrollBar() : NULL);
	});
	mOutputPlainWidget->hide();
	mOutputBackgroundOverlay = new QLabel(mOutputWidget->viewport());
	mOutputBackgroundOverlay->setObjectName("OutputBackgroundOverlay");
	mOutputBackgroundOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mOutputBackgroundOverlay->setScaledContents(false);
	mOutputBackgroundOverlay->setAlignment(Qt::AlignCenter);
	mOutputBackgroundOverlay->hide();
	mOutputBackgroundOverlay->lower();
	mOutputPanel = new QWidget(this);
	mOutputPanel->setObjectName("OutputPanel");
	QVBoxLayout* OutputPanelLayout = new QVBoxLayout(mOutputPanel);
	OutputPanelLayout->setContentsMargins(0, 0, 0, 0);
	OutputPanelLayout->setSpacing(0);
	mOutputTabs = new QTabBar(mOutputPanel);
	mOutputTabs->setObjectName("CategoryTabs");
	mOutputTabs->setDocumentMode(true);
	mOutputTabs->setExpanding(false);
	mOutputTabs->setDrawBase(false);
	mOutputTabs->setUsesScrollButtons(true);
	mOutputTabs->setElideMode(Qt::ElideRight);
	mOutputTabs->addTab("Full");
	mOutputTabs->addTab("Errors");
	mOutputTabs->addTab("Warnings");
	mOutputTabs->addTab("Materials");
	mOutputTabs->addTab("Images");
	mOutputTabs->addTab("xmodels");
	mOutputTabs->addTab("playercharacters");
	mOutputTabs->addTab("converting/mesh");
	mOutputTabs->addTab("techsets");
	mOutputTabs->addTab("shaders");
	mOutputTabs->setCurrentIndex(OutputLogTabFull);
	connect(mOutputTabs, &QTabBar::currentChanged, this, [=](int Index)
	{
		mOutputTabIndex = qMax(0, Index);
		RebuildOutputFromBuffer();
	});
	mLogFiltersButton = new QToolButton(this);
	mLogFiltersButton->setObjectName("LogFiltersButton");
	mLogFiltersButton->setText("Log Filters");
	mLogFiltersButton->setPopupMode(QToolButton::InstantPopup);
	QMenu* LogFiltersMenu = new QMenu(mLogFiltersButton);
	struct LogFilterMenuEntry
	{
		LogFilterKind Kind;
		const char* Label;
	};
	const LogFilterMenuEntry LogFilterEntries[] =
	{
		{ LogFilterLeakMod, "Show L3akMod" },
		{ LogFilterPhilLibX, "Show PhilLibX" },
		{ LogFilterGdtDb, "Show gdtDB" },
		{ LogFilterLinking, "Show Linking / done" },
		{ LogFilterCommands, "Show commands" },
		{ LogFilterWarnings, "Show warnings" },
		{ LogFilterErrors, "Show errors" }
	};
	for (int EntryIdx = 0; EntryIdx < static_cast<int>(sizeof(LogFilterEntries) / sizeof(LogFilterEntries[0])); EntryIdx++)
	{
		const LogFilterMenuEntry Entry = LogFilterEntries[EntryIdx];
		QAction* FilterAction = LogFiltersMenu->addAction(Entry.Label);
		FilterAction->setCheckable(true);
		FilterAction->setChecked(IsLogFilterEnabled(Settings, Entry.Kind));
		connect(FilterAction, &QAction::toggled, this, [Entry](bool Enabled)
		{
			QSettings FilterSettings;
			FilterSettings.setValue(LogFilterSettingKey(Entry.Kind), Enabled);
		});
	}
	mLogFiltersButton->setMenu(LogFiltersMenu);
	mLogSelectionButton = new QToolButton(this);
	mLogSelectionButton->setObjectName("LogFiltersButton");
	mLogSelectionButton->setText("Text Select");
	mLogSelectionButton->setCheckable(true);
	mLogSelectionButton->setToolTip("Switch to a full-text console view for drag selection and copy.");
	connect(mLogSelectionButton, &QToolButton::toggled, this, [=](bool Enabled)
	{
		mOutputSelectionMode = Enabled;
		UpdateOutputConsoleMode();
	});
	OutputPanelLayout->addWidget(mOutputTabs, 0);
	OutputPanelLayout->addWidget(mOutputWidget, 1);
	OutputPanelLayout->addWidget(mOutputPlainWidget, 1);
	ApplyLauncherLayout();

	setCentralWidget(mCentralWidgetSplitter);
	QLabel* FooterVersionLabel = new QLabel("Current update version: 1.0.0", this);
	FooterVersionLabel->setObjectName("FooterStatusLabel");
	QLabel* FooterCreditsLabel = new QLabel("Edits by Sphynx", this);
	FooterCreditsLabel->setObjectName("FooterStatusLabel");
	statusBar()->addWidget(FooterVersionLabel, 0);
	statusBar()->addPermanentWidget(mLogSelectionButton, 0);
	statusBar()->addPermanentWidget(mLogFiltersButton, 0);
	statusBar()->addPermanentWidget(FooterCreditsLabel, 0);

	mShippedMapList << "mp_aerospace" <<  "mp_apartments" << "mp_arena" << "mp_banzai" << "mp_biodome" << "mp_chinatown" << "mp_city" << "mp_conduit" << "mp_crucible" << "mp_cryogen" << "mp_ethiopia" << "mp_freerun_01" << "mp_freerun_02" << "mp_freerun_03" << "mp_freerun_04" << "mp_havoc" << "mp_infection" << "mp_kung_fu" << "mp_metro" << "mp_miniature" << "mp_nuketown_x" << "mp_redwood" << "mp_rise" << "mp_rome" << "mp_ruins" << "mp_sector" << "mp_shrine" << "mp_skyjacked" << "mp_spire" << "mp_stronghold" << "mp_veiled" << "mp_waterpark" << "mp_western" << "zm_castle" << "zm_factory" << "zm_genesis" << "zm_island" << "zm_levelcommon" << "zm_stalingrad" << "zm_zod";

	Settings.beginGroup("MainWindow");
	resize(QSize(980, 660));
	move(QPoint(200, 200));
	restoreGeometry(Settings.value("Geometry").toByteArray());
	restoreState(Settings.value("State").toByteArray());
	Settings.endGroup();

	SteamAPI_Init();

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(SteamUpdate()));
	mTimer.start(1500);
	UpdateGameRunningState();

	UpdateTheme();
	PopulateFileList();
}

mlMainWindow::~mlMainWindow()
{
}

void mlMainWindow::EnsureThemeProfiles()
{
	QSettings Settings;
	const QStringList BuiltInIds = QStringList() << "original-updated" << "original-classic" << "dark-modern";
	for (const QString& ThemeProfileId : BuiltInIds)
	{
		Settings.beginGroup(kThemeProfilesGroup);
		Settings.beginGroup(ThemeProfileId);
		if (!Settings.contains("Name"))
			Settings.setValue("Name", ThemeProfileDisplayNameForBuiltInId(ThemeProfileId));
		const QVariantMap DefaultValues = DefaultThemeProfileValues(ThemeProfileId);
		for (auto It = DefaultValues.constBegin(); It != DefaultValues.constEnd(); ++It)
		{
			if (!Settings.contains(It.key()))
				Settings.setValue(It.key(), It.value());
		}
		Settings.endGroup();
		Settings.endGroup();
	}

	if (!Settings.contains(kThemeProfileSettingKey))
		Settings.setValue(kThemeProfileSettingKey, BuiltInThemeProfileId(ThemeModeFromSettings(Settings)));
}

QString mlMainWindow::CurrentThemeProfileId() const
{
	QSettings Settings;
	QString ThemeProfileId = Settings.value(kThemeProfileSettingKey, BuiltInThemeProfileId(mThemeMode)).toString().trimmed().toLower();
	return ThemeProfileId.isEmpty() ? QString("original-updated") : ThemeProfileId;
}

QStringList mlMainWindow::AvailableThemeProfileIds() const
{
	QSettings Settings;
	QStringList ThemeProfileIds = QStringList() << "original-updated" << "original-classic" << "dark-modern";
	Settings.beginGroup(kThemeProfilesGroup);
	const QStringList SavedIds = Settings.childGroups();
	Settings.endGroup();
	for (const QString& SavedId : SavedIds)
	{
		if (!ThemeProfileIds.contains(SavedId))
			ThemeProfileIds << SavedId;
	}
	return ThemeProfileIds;
}

QString mlMainWindow::ThemeProfileDisplayName(const QString& ThemeProfileId) const
{
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	const QString Name = Settings.value("Name", ThemeProfileDisplayNameForBuiltInId(ThemeProfileId)).toString().trimmed();
	Settings.endGroup();
	Settings.endGroup();
	return Name.isEmpty() ? ThemeProfileDisplayNameForBuiltInId(ThemeProfileId) : Name;
}

QVariantMap mlMainWindow::ThemeProfileValues(const QString& ThemeProfileId) const
{
	QVariantMap Values = DefaultThemeProfileValues(ThemeProfileId);
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Settings.contains(Key))
			Values.insert(Key, Settings.value(Key));
	}
	Settings.endGroup();
	Settings.endGroup();
	return Values;
}

void mlMainWindow::SaveThemeProfile(const QString& ThemeProfileId, const QString& DisplayName, const QVariantMap& Values)
{
	QSettings Settings;
	Settings.beginGroup(kThemeProfilesGroup);
	Settings.beginGroup(ThemeProfileId);
	Settings.setValue("Name", DisplayName.trimmed().isEmpty() ? ThemeProfileDisplayNameForBuiltInId(ThemeProfileId) : DisplayName.trimmed());
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Values.contains(Key))
			Settings.setValue(Key, Values.value(Key));
	}
	Settings.endGroup();
	Settings.endGroup();
}

void mlMainWindow::ApplyThemeProfile(const QString& ThemeProfileId)
{
	const QVariantMap Values = ThemeProfileValues(ThemeProfileId);
	QSettings Settings;
	for (const QString& Key : ThemeProfileSettingKeys())
	{
		if (Values.contains(Key))
			Settings.setValue(Key, Values.value(Key));
	}
	Settings.setValue(kThemeProfileSettingKey, ThemeProfileId);
	mThemeProfileId = ThemeProfileId;
	mThemeMode = ThemeModeFromSettings(Settings);
	mLauncherLayout = Settings.value("LauncherLayout", mLauncherLayout).toString().trimmed().toLower();
}

void mlMainWindow::ShowOnlineLaunchProgressDialog(const QString& TargetLabel)
{
	mPendingOnlineLaunchFeedback = true;
	mPendingOnlineLaunchLabel = TargetLabel.trimmed().isEmpty() ? QString("selected content") : TargetLabel.trimmed();

	if (!mLaunchProgressDialog)
	{
		mLaunchProgressDialog = new QProgressDialog(this);
		mLaunchProgressDialog->setWindowTitle("Starting Game");
		mLaunchProgressDialog->setMinimumDuration(0);
		mLaunchProgressDialog->setRange(0, 0);
		mLaunchProgressDialog->setAutoClose(false);
		mLaunchProgressDialog->setAutoReset(false);
		mLaunchProgressDialog->setCancelButtonText("Hide");
		mLaunchProgressDialog->setWindowModality(Qt::WindowModal);
		mLaunchProgressDialog->resize(720, 160);
		if (ThemeUsesClassicChrome(mThemeMode))
		{
			mLaunchProgressDialog->setStyleSheet(
				"QProgressDialog { background: #5a5a5a; color: #111111; }"
				"QLabel { color: #111111; font-weight: 600; }"
				"QProgressBar { background: #4a4a4a; border: 1px solid #3a3a3a; border-radius: 10px; min-height: 22px; color: #111111; }"
				"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }"
				"QPushButton { background: #4a4a4a; border: 1px solid #3a3a3a; border-radius: 8px; padding: 8px 14px; color: #111111; }");
		}
		else if (ThemeUsesDarkModernChrome(mThemeMode))
		{
			mLaunchProgressDialog->setStyleSheet(
				"QProgressDialog { background: #111418; color: #eef1f4; }"
				"QLabel { color: #eef1f4; font-weight: 600; }"
				"QProgressBar { background: #1a2026; border: 1px solid #2d353d; border-radius: 10px; min-height: 22px; }"
				"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }"
				"QPushButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; padding: 8px 14px; color: #eef1f4; }");
		}
		connect(mLaunchProgressDialog, &QProgressDialog::canceled, this, [=]()
		{
			if (mLaunchProgressDialog)
				mLaunchProgressDialog->hide();
		});
	}

	mLaunchProgressDialog->setLabelText(
		QString("Starting %1 through Steam online mode...\n\nThis can take a bit while Steam hands off to Black Ops 3. The launcher will update automatically once the game begins opening.")
			.arg(mPendingOnlineLaunchLabel));
	mLaunchProgressDialog->show();
}

void mlMainWindow::CloseOnlineLaunchProgressDialog()
{
	mPendingOnlineLaunchFeedback = false;
	mPendingOnlineLaunchLabel.clear();
	if (mLaunchProgressDialog)
		mLaunchProgressDialog->hide();
}

void mlMainWindow::CreateActions()
{
	mActionFileNew = new QAction(QIcon(":/resources/FileNew.png"), "&New...", this);
	mActionFileNew->setShortcut(QKeySequence("Ctrl+N"));
	connect(mActionFileNew, SIGNAL(triggered()), this, SLOT(OnFileNew()));

	mActionFileAssetEditor = new QAction(QIcon(":/resources/AssetEditor.png"), "&Asset Editor", this);
	mActionFileAssetEditor->setShortcut(QKeySequence("Ctrl+A"));
	connect(mActionFileAssetEditor, SIGNAL(triggered()), this, SLOT(OnFileAssetEditor()));

	mActionFileLevelEditor = new QAction(QIcon(":/resources/Radiant.png"), "Open in &Radiant", this);
	mActionFileLevelEditor->setShortcut(QKeySequence("Ctrl+R"));
	mActionFileLevelEditor->setToolTip("Level Editor");
	connect(mActionFileLevelEditor, SIGNAL(triggered()), this, SLOT(OnFileLevelEditor()));

	mActionFileExport2Bin = new QAction(QIcon(":/resources/Export2Bin.png"), "&Export2Bin GUI", this);
	mActionFileExport2Bin->setShortcut(QKeySequence("Ctrl+E"));
	connect(mActionFileExport2Bin, SIGNAL(triggered()), this, SLOT(OnFileExport2Bin()));

	mActionFileExit = new QAction("E&xit", this);
	connect(mActionFileExit, SIGNAL(triggered()), this, SLOT(close()));

	mActionEditBuild = new QAction(QIcon(":/resources/Go.png"), "Build", this);
	mActionEditBuild->setShortcut(QKeySequence("Ctrl+B"));
	connect(mActionEditBuild, SIGNAL(triggered()), this, SLOT(OnEditBuild()));

	mActionEditBuildAllLanguages = new QAction("Build (English)", this);
	mActionEditBuildAllLanguages->setShortcut(QKeySequence("Ctrl+Shift+B"));
	connect(mActionEditBuildAllLanguages, SIGNAL(triggered()), this, SLOT(OnEditBuildAllLanguages()));

	mActionEditReadyForPublish = new QAction(QIcon(":/resources/upload.png"), "Ready for Publish", this);
	connect(mActionEditReadyForPublish, SIGNAL(triggered()), this, SLOT(OnEditReadyForPublish()));

	mActionEditPublish = new QAction(QIcon(":/resources/upload.png"), "Publish", this);
	mActionEditPublish->setShortcut(QKeySequence("Ctrl+P"));
	connect(mActionEditPublish, SIGNAL(triggered()), this, SLOT(OnEditPublish()));

	mActionEditOptions = new QAction("&Options...", this);
	connect(mActionEditOptions, SIGNAL(triggered()), this, SLOT(OnEditOptions()));

	mActionHelpAbout = new QAction("&About...", this);
	connect(mActionHelpAbout, SIGNAL(triggered()), this, SLOT(OnHelpAbout()));
}

void mlMainWindow::CreateMenu()
{
	QMenuBar* MenuBar = new QMenuBar(this);

	QMenu* FileMenu = new QMenu("&File", MenuBar);
	FileMenu->addAction(mActionFileNew);
	FileMenu->addSeparator();
	FileMenu->addAction(mActionFileAssetEditor);
	FileMenu->addAction(mActionFileLevelEditor);
	FileMenu->addAction(mActionFileExport2Bin);
	FileMenu->addSeparator();
	FileMenu->addAction(mActionFileExit);
	MenuBar->addAction(FileMenu->menuAction());

	QMenu* EditMenu = new QMenu("&Edit", MenuBar);
	EditMenu->addAction(mActionEditBuild);
	EditMenu->addAction(mActionEditBuildAllLanguages);
	EditMenu->addAction(mActionEditReadyForPublish);
	EditMenu->addAction(mActionEditPublish);
	MenuBar->addAction(EditMenu->menuAction());

	QMenu* ThemesMenu = new QMenu("&Themes", MenuBar);
	QActionGroup* ThemeGroup = new QActionGroup(ThemesMenu);
	ThemeGroup->setExclusive(true);
		mActionThemeModern = ThemesMenu->addAction("Original Updated", this, SLOT(OnSetModernTheme()));
		mActionThemeClassic = ThemesMenu->addAction("Original Classic", this, SLOT(OnSetClassicTheme()));
		mActionThemeDarkModern = ThemesMenu->addAction("Dark Modern", this, SLOT(OnSetDarkModernTheme()));
	mActionThemeModern->setCheckable(true);
	mActionThemeDarkModern->setCheckable(true);
	mActionThemeClassic->setCheckable(true);
		ThemeGroup->addAction(mActionThemeModern);
		ThemeGroup->addAction(mActionThemeClassic);
		ThemeGroup->addAction(mActionThemeDarkModern);
	MenuBar->addAction(ThemesMenu->menuAction());

	QMenu* OptionsMenu = new QMenu("&Options", MenuBar);
	OptionsMenu->addAction(mActionEditOptions);
	MenuBar->addAction(OptionsMenu->menuAction());

	QMenu* HelpMenu = new QMenu("&Help", MenuBar);
	HelpMenu->addAction(mActionHelpAbout);
	MenuBar->addAction(HelpMenu->menuAction());

	setMenuBar(MenuBar);
	UpdateThemeMenuChecks();
}

void mlMainWindow::CreateToolBar()
{
	QToolBar* ToolBar = new QToolBar("Standard", this);
	ToolBar->setObjectName(QStringLiteral("StandardToolBar"));
	ToolBar->setMovable(false);
	ToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ToolBar->setIconSize(QSize(20, 20));
	ToolBar->setContentsMargins(0, 4, 0, 2);

	ToolBar->addAction(mActionFileNew);
	ToolBar->addAction(mActionEditBuild);
	ToolBar->addAction(mActionEditReadyForPublish);
	ToolBar->addAction(mActionEditPublish);
	ToolBar->addSeparator();
	ToolBar->addAction(mActionFileAssetEditor);
	ToolBar->addAction(mActionFileLevelEditor);
	ToolBar->addAction(mActionFileExport2Bin);

	addToolBar(Qt::TopToolBarArea, ToolBar);

	for (QToolButton* Button : ToolBar->findChildren<QToolButton*>())
	{
		Button->setMinimumWidth(76);
		Button->setMinimumHeight(48);
		Button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}
}

void mlMainWindow::InitExport2BinGUI()
{
	QDockWidget *dock = new QDockWidget(this);
	dock->setWindowTitle("Export2Bin");
	dock->setFloating(true);

	QWidget* widget = new QWidget(dock);
	QGridLayout* gridLayout = new QGridLayout();
	widget->setLayout(gridLayout);
	dock->setWidget(widget);

	Export2BinGroupBox* groupBox = new Export2BinGroupBox(dock, this);
	gridLayout->addWidget(groupBox, 0, 0);

	QLabel* label = new QLabel("Drag Files Here", groupBox);
	label->setAlignment(Qt::AlignCenter);
	QVBoxLayout* groupBoxLayout = new QVBoxLayout(groupBox);
	groupBoxLayout->addWidget(label);
	groupBox->setLayout(groupBoxLayout);

	mExport2BinOverwriteWidget = new QCheckBox("&Overwrite Existing Files", widget);
	gridLayout->addWidget(mExport2BinOverwriteWidget, 1, 0);
	
	QSettings Settings;
	mExport2BinOverwriteWidget->setChecked(Settings.value("Export2Bin_OverwriteFiles", true).toBool());

	QHBoxLayout* dirLayout = new QHBoxLayout();
	QLabel* dirLabel = new QLabel("Ouput Directory:", widget);
	mExport2BinTargetDirWidget = new QLineEdit(widget);
	QToolButton* dirBrowseButton = new QToolButton(widget);
	dirBrowseButton->setText("...");

	const QDir defaultPath = QString("%1/model_export/export2bin/").arg(mToolsPath);
	mExport2BinTargetDirWidget->setText(Settings.value("Export2Bin_TargetDir", defaultPath.absolutePath()).toString());

	connect(dirBrowseButton, SIGNAL(clicked()), this, SLOT(OnExport2BinChooseDirectory()));
	connect(mExport2BinOverwriteWidget, SIGNAL(clicked()), this, SLOT(OnExport2BinToggleOverwriteFiles()));

	dirBrowseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	dirLayout->addWidget(dirLabel);
	dirLayout->addWidget(mExport2BinTargetDirWidget);
	dirLayout->addWidget(dirBrowseButton);

	gridLayout->addLayout(dirLayout, 2, 0);

	groupBox->setAcceptDrops(true);

	dock->resize(QSize(256, 256));

	mExport2BinGUIWidget = dock;
}

void mlMainWindow::closeEvent(QCloseEvent* Event)
{
	QSettings Settings;
	Settings.beginGroup("MainWindow");
	Settings.setValue("Geometry", saveGeometry());
	Settings.setValue("State", saveState());
	Settings.endGroup();

	Event->accept();
}

bool mlMainWindow::eventFilter(QObject* Watched, QEvent* Event)
{
	if ((Watched == mOutputWidget || Watched == (mOutputWidget ? mOutputWidget->viewport() : NULL)) && Event->type() == QEvent::KeyPress)
	{
		QKeyEvent* KeyEvent = static_cast<QKeyEvent*>(Event);
		if (KeyEvent->matches(QKeySequence::SelectAll) && mOutputWidget)
		{
			mOutputWidget->selectAll();
			return true;
		}

		if (KeyEvent->matches(QKeySequence::Copy) && mOutputWidget)
		{
			QStringList SelectedBlocks;
			const QList<QTreeWidgetItem*> SelectedItems = mOutputWidget->selectedItems();
			for (QTreeWidgetItem* Item : SelectedItems)
			{
				const QString BlockText = LogBlockText(Item);
				if (!BlockText.isEmpty())
					SelectedBlocks.append(BlockText);
			}

			if (!SelectedBlocks.isEmpty())
				QApplication::clipboard()->setText(SelectedBlocks.join("\n\n"));
			return true;
		}
	}

	if ((Watched == (mFileListWidget ? mFileListWidget->viewport() : NULL)
		|| Watched == (mOutputWidget ? mOutputWidget->viewport() : NULL)
		|| Watched == (mOutputPlainWidget ? mOutputPlainWidget->viewport() : NULL))
		&& (Event->type() == QEvent::Resize || Event->type() == QEvent::Show))
	{
		UpdateBackgroundOverlays();
	}

	return QMainWindow::eventFilter(Watched, Event);
}

void mlMainWindow::SteamUpdate()
{
	SteamAPI_RunCallbacks();
	static int PollCounter = 0;
	if (++PollCounter >= 4)
	{
		UpdateGameRunningState();
		PollCounter = 0;
	}
}

bool mlMainWindow::IsTrackedGameProcessAlive() const
{
	if (!mGameProcessId)
		return false;

	HANDLE ProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(mGameProcessId));
	if (!ProcessHandle)
		return false;

	const DWORD WaitResult = WaitForSingleObject(ProcessHandle, 0);
	CloseHandle(ProcessHandle);
	return WaitResult == WAIT_TIMEOUT;
}

mlMainWindow::GameRunningState mlMainWindow::DetectGameRunningState()
{
	if (IsTrackedGameProcessAlive())
	{
		GameWindowSearchState SearchState = { static_cast<DWORD>(mGameProcessId), false };
		EnumWindows(FindVisibleWindowForProcess, reinterpret_cast<LPARAM>(&SearchState));
		return SearchState.HasVisibleWindow ? GameRunning : GameStarting;
	}

	mGameProcessId = 0;
	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snapshot == INVALID_HANDLE_VALUE)
		return GameNotRunning;

	PROCESSENTRY32 ProcessEntry;
	ProcessEntry.dwSize = sizeof(PROCESSENTRY32);
	GameRunningState State = GameNotRunning;
	if (Process32First(Snapshot, &ProcessEntry))
	{
		do
		{
			if (_stricmp(ProcessEntry.szExeFile, "BlackOps3.exe") == 0)
			{
				mGameProcessId = ProcessEntry.th32ProcessID;
				GameWindowSearchState SearchState = { ProcessEntry.th32ProcessID, false };
				EnumWindows(FindVisibleWindowForProcess, reinterpret_cast<LPARAM>(&SearchState));
				State = SearchState.HasVisibleWindow ? GameRunning : GameStarting;
				break;
			}
		} while (Process32Next(Snapshot, &ProcessEntry));
	}

	CloseHandle(Snapshot);
	return State;
}

bool mlMainWindow::IsGameRunning() const
{
	return mGameRunningState != GameNotRunning;
}

void mlMainWindow::UpdateGameRunningState()
{
	if (!mCloseGameButton || !mCloseGameStatusLabel)
		return;

	mGameRunningState = DetectGameRunningState();
	const bool Running = (mGameRunningState != GameNotRunning);
	mCachedGameRunning = Running;
	mCloseGameButton->setEnabled(Running);

	if (mGameRunningState == GameRunning)
	{
		mCloseGameButton->setText("Close Game");
		mCloseGameButton->setEnabled(true);
		mCloseGameStatusLabel->setText("Game fully open");
		mCloseGameStatusLabel->setStyleSheet("color: #44d17a; font-weight: 700;");
		CloseOnlineLaunchProgressDialog();
	}
	else if (mGameRunningState == GameStarting)
	{
		mCloseGameButton->setText("Close Game");
		mCloseGameButton->setEnabled(true);
		mCloseGameStatusLabel->setText("Game starting up");
		mCloseGameStatusLabel->setStyleSheet("color: #ff9b42;");
		if (mPendingOnlineLaunchFeedback && mLaunchProgressDialog)
		{
			mLaunchProgressDialog->setLabelText(
				QString("Steam launch requested for %1...\n\nBlack Ops 3 is starting up now. This window will close automatically once the game is fully open.")
					.arg(mPendingOnlineLaunchLabel.isEmpty() ? QString("selected content") : mPendingOnlineLaunchLabel));
			mLaunchProgressDialog->show();
		}
	}
	else
	{
		if (mPendingOnlineLaunchFeedback && mLaunchProgressDialog)
		{
			mLaunchProgressDialog->setLabelText(
				QString("Waiting for Steam to start %1...\n\nIf this takes longer than expected, Steam may still be processing the online launch request.")
					.arg(mPendingOnlineLaunchLabel.isEmpty() ? QString("selected content") : mPendingOnlineLaunchLabel));
			mLaunchProgressDialog->show();
		}
		QTreeWidgetItem* CurrentItem = mFileListWidget ? mFileListWidget->currentItem() : NULL;
		const int ItemType = CurrentItem ? CurrentItem->data(0, Qt::UserRole).toInt() : ML_ITEM_UNKNOWN;
		if (ItemType == ML_ITEM_MAP)
		{
			mCloseGameButton->setText("Start Map");
			mCloseGameButton->setEnabled(true);
			mCloseGameStatusLabel->setText("Launch selected map");
		}
		else if (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP)
		{
			mCloseGameButton->setText("Start Mod");
			mCloseGameButton->setEnabled(true);
			mCloseGameStatusLabel->setText("Launch selected mod");
		}
		else
		{
			mCloseGameButton->setText("Close Game");
			mCloseGameButton->setEnabled(false);
			mCloseGameStatusLabel->setText("Game is not running");
		}
		mCloseGameStatusLabel->setStyleSheet(QString());
	}
}

QString mlMainWindow::SectionSettingKey(const QString& SectionName) const
{
	QString Normalized = SectionName.toLower();
	Normalized.replace(' ', '_');
	return QString("Sections/%1").arg(Normalized);
}

QString mlMainWindow::LauncherDataRoot() const
{
	return QDir::cleanPath(QString("%1/modtools/launcher_data").arg(mGamePath));
}

QString mlMainWindow::NotesFilePath(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	const bool IsMap = (ItemType == ML_ITEM_MAP);
	const QString TargetName = SanitizedFileName(IsMap ? EntryName : ContainerName);
	return QDir::cleanPath(QString("%1/notes/%2/%3.json").arg(LauncherDataRoot(), IsMap ? "maps" : "mods", TargetName));
}

QString mlMainWindow::WorkshopVersionsRoot(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	const bool IsMap = (ItemType == ML_ITEM_MAP);
	const QString TargetName = SanitizedFileName(IsMap ? EntryName : ContainerName);
	return QDir::cleanPath(QString("%1/workshop_versions/%2/%3").arg(LauncherDataRoot(), IsMap ? "maps" : "mods", TargetName));
}

QString mlMainWindow::ActiveWorkshopFolder(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MAP)
		return QDir::cleanPath(QString("%1/usermaps/%2/zone").arg(mGamePath, EntryName));

	return QDir::cleanPath(QString("%1/mods/%2/zone").arg(mGamePath, ContainerName));
}

bool mlMainWindow::EditNotesForItem(QTreeWidgetItem* Item)
{
	if (!Item)
		return false;

	const int ItemType = Item->data(0, Qt::UserRole).toInt();
	if (ItemType != ML_ITEM_MAP && ItemType != ML_ITEM_MOD && ItemType != ML_ITEM_MOD_GROUP)
		return false;

	const QString ContainerName = GetItemContainerName(Item);
	const QString EntryName = GetItemEntryName(Item);
	const QString DisplayName = (ItemType == ML_ITEM_MAP) ? EntryName : ContainerName;
	const QString FilePath = NotesFilePath(ItemType, ContainerName, EntryName);

	QJsonObject ExistingRoot;
	QFile ExistingFile(FilePath);
	if (ExistingFile.open(QIODevice::ReadOnly))
		ExistingRoot = QJsonDocument::fromJson(ExistingFile.readAll()).object();

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle(QString("Notes - %1").arg(DisplayName));
	Dialog.resize(760, 620);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QTabWidget* Tabs = new QTabWidget(&Dialog);
	Layout->addWidget(Tabs, 1);

	auto MakeTab = [&](const QString& Title, const QString& Key) -> QTextEdit*
	{
		QTextEdit* Editor = new QTextEdit(&Dialog);
		Editor->setAcceptRichText(false);
		Editor->setPlainText(ExistingRoot.value(Key).toString());
		Tabs->addTab(Editor, Title);
		return Editor;
	};

	QTextEdit* GeneralNotes = MakeTab("General", "general");
	QTextEdit* CompileNotes = MakeTab("Compile", "compile");
	QTextEdit* PublishNotes = MakeTab("Publish", "publish");

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(ButtonBox);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	QDir().mkpath(QFileInfo(FilePath).absolutePath());
	QJsonObject Root;
	Root["general"] = GeneralNotes->toPlainText().trimmed();
	Root["compile"] = CompileNotes->toPlainText().trimmed();
	Root["publish"] = PublishNotes->toPlainText().trimmed();
	Root["updated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
	QFile File(FilePath);
	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Notes", QString("Unable to save notes to '%1'.").arg(FilePath));
		return false;
	}

	File.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

bool mlMainWindow::SaveWorkshopVersionSnapshot(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& SourceWorkshopJsonPath, const QString& VersionLabel, const QString& OverridePublisherId)
{
	QFile SourceFile(SourceWorkshopJsonPath);
	if (!SourceFile.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, "Workshop Versions", QString("Unable to open '%1'.").arg(SourceWorkshopJsonPath));
		return false;
	}

	QJsonObject Root = QJsonDocument::fromJson(SourceFile.readAll()).object();
	const QString PublisherId = OverridePublisherId.trimmed().isEmpty() ? Root.value("PublisherID").toString().trimmed() : OverridePublisherId.trimmed();
	const QString FolderName = SanitizedFileName(VersionLabel.isEmpty() ? (PublisherId.isEmpty() ? QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") : PublisherId) : VersionLabel);
	const QString VersionFolder = QDir::cleanPath(QString("%1/%2").arg(WorkshopVersionsRoot(ItemType, ContainerName, EntryName), FolderName));
	QDir().mkpath(VersionFolder);
	Root["LauncherVersionLabel"] = VersionLabel.trimmed().isEmpty() ? HumanizedVersionLabel(FolderName) : VersionLabel.trimmed();

	QString ThumbnailPath = Root.value("Thumbnail").toString().trimmed();
	if (!ThumbnailPath.isEmpty() && QFileInfo(ThumbnailPath).isRelative())
		ThumbnailPath = QDir(QFileInfo(SourceWorkshopJsonPath).absolutePath()).filePath(ThumbnailPath);
	if (!ThumbnailPath.isEmpty() && !QFileInfo(ThumbnailPath).isFile())
	{
		const QString FallbackThumbPath = QDir(QFileInfo(SourceWorkshopJsonPath).absolutePath()).filePath(QFileInfo(ThumbnailPath).fileName());
		if (QFileInfo(FallbackThumbPath).isFile())
			ThumbnailPath = FallbackThumbPath;
	}
	if (!ThumbnailPath.isEmpty() && !QFileInfo(ThumbnailPath).isFile())
	{
		const QString ReplacementThumb = QFileDialog::getOpenFileName(this, "Select Thumbnail For Workshop Version", QFileInfo(SourceWorkshopJsonPath).absolutePath(), "Images (*.png *.jpg *.jpeg *.gif *.bmp)");
		if (!ReplacementThumb.isEmpty())
			ThumbnailPath = ReplacementThumb;
	}

	if (!ThumbnailPath.isEmpty() && QFileInfo(ThumbnailPath).isFile())
	{
		const QString ThumbFileName = QFileInfo(ThumbnailPath).fileName();
		QFile::remove(QDir::cleanPath(QString("%1/%2").arg(VersionFolder, ThumbFileName)));
		QFile::copy(ThumbnailPath, QDir::cleanPath(QString("%1/%2").arg(VersionFolder, ThumbFileName)));
		Root["Thumbnail"] = ThumbFileName;
	}

	if (!PublisherId.isEmpty())
		Root["PublisherID"] = PublisherId;

	QFile OutFile(QDir::cleanPath(QString("%1/workshop.json").arg(VersionFolder)));
	if (!OutFile.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Workshop Versions", QString("Unable to save version into '%1'.").arg(VersionFolder));
		return false;
	}

	OutFile.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

bool mlMainWindow::ActivateWorkshopVersion(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& VersionFolderPath)
{
	const QString SourceJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(VersionFolderPath));
	QFile SourceFile(SourceJsonPath);
	if (!SourceFile.open(QIODevice::ReadOnly))
		return false;

	QJsonObject Root = QJsonDocument::fromJson(SourceFile.readAll()).object();
	const QString TargetFolder = ActiveWorkshopFolder(ItemType, ContainerName, EntryName);
	QDir().mkpath(TargetFolder);

	const QString StoredThumbnail = Root.value("Thumbnail").toString().trimmed();
	if (!StoredThumbnail.isEmpty())
	{
		const QString StoredThumbnailPath = QDir::cleanPath(QString("%1/%2").arg(VersionFolderPath, StoredThumbnail));
		if (QFileInfo(StoredThumbnailPath).isFile())
		{
			const QString TargetThumbnailPath = QDir::cleanPath(QString("%1/%2").arg(TargetFolder, QFileInfo(StoredThumbnail).fileName()));
			QFile::remove(TargetThumbnailPath);
			QFile::copy(StoredThumbnailPath, TargetThumbnailPath);
			Root["Thumbnail"] = TargetThumbnailPath;
		}
	}

	QFile TargetFile(QDir::cleanPath(QString("%1/workshop.json").arg(TargetFolder)));
	if (!TargetFile.open(QIODevice::WriteOnly))
		return false;

	TargetFile.write(QJsonDocument(Root).toJson(QJsonDocument::Indented));
	return true;
}

void mlMainWindow::ShowWorkshopVersionsDialog(QTreeWidgetItem* Item)
{
	if (!Item)
		return;

	const int RawItemType = Item->data(0, Qt::UserRole).toInt();
	const int ItemType = (RawItemType == ML_ITEM_MAP) ? ML_ITEM_MAP : ML_ITEM_MOD_GROUP;
	const QString ContainerName = GetItemContainerName(Item);
	const QString EntryName = GetItemEntryName(Item);
	const QString ActiveFolder = ActiveWorkshopFolder(ItemType, ContainerName, EntryName);
	const QString VersionsRoot = WorkshopVersionsRoot(ItemType, ContainerName, EntryName);
	QDir().mkpath(VersionsRoot);

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Workshop Versions");
	Dialog.resize(760, 480);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	const QString FavoriteKey = (ItemType == ML_ITEM_MAP)
		? QString("map:%1").arg(EntryName.toLower())
		: QString("mod:%1").arg(ContainerName.toLower());
	const QString InternalName = (ItemType == ML_ITEM_MAP) ? EntryName : ContainerName;
	const QString DisplayName = DisplayNameForEntry(FavoriteKey);
	const QString HeaderName = DisplayName.isEmpty() ? InternalName : QString("%1 [%2]").arg(DisplayName, InternalName);
	Layout->addWidget(new QLabel(QString("Backups for %1").arg(HeaderName)));
	QListWidget* VersionsList = new QListWidget(&Dialog);
	VersionsList->setObjectName("WorkshopVersionsList");
	VersionsList->setSpacing(6);
	VersionsList->setMouseTracking(true);
	VersionsList->setSelectionMode(QAbstractItemView::SingleSelection);
	VersionsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	VersionsList->setFocusPolicy(Qt::NoFocus);
	Layout->addWidget(VersionsList, 1);

	auto ReadWorkshopObject = [](const QString& WorkshopJsonPath) -> QJsonObject
	{
		QFile WorkshopFile(WorkshopJsonPath);
		if (!WorkshopFile.open(QIODevice::ReadOnly))
			return QJsonObject();
		return QJsonDocument::fromJson(WorkshopFile.readAll()).object();
	};

	auto WorkshopVersionIsActive = [&](const QString& FolderPath) -> bool
	{
		const QString ActiveJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		const QString VersionJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(FolderPath));
		QFile ActiveFile(ActiveJsonPath);
		QFile VersionFile(VersionJsonPath);
		if (!ActiveFile.open(QIODevice::ReadOnly) || !VersionFile.open(QIODevice::ReadOnly))
			return false;
		return ActiveFile.readAll() == VersionFile.readAll();
	};

	auto RefreshVersions = [&]()
	{
		VersionsList->clear();
		const QStringList Folders = QDir(VersionsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		QString ActiveFolderPath;
		for (const QString& Folder : Folders)
		{
			const QString FolderPath = QDir::cleanPath(QString("%1/%2").arg(VersionsRoot, Folder));
			QListWidgetItem* ListItem = new QListWidgetItem(VersionsList);
			ListItem->setData(Qt::UserRole, FolderPath);
			ListItem->setSizeHint(QSize(0, 96));

			const QJsonObject Root = ReadWorkshopObject(QDir::cleanPath(QString("%1/workshop.json").arg(FolderPath)));
			const bool IsActive = WorkshopVersionIsActive(FolderPath);
			if (IsActive)
				ActiveFolderPath = FolderPath;

			QWidget* RowWidget = new QWidget(VersionsList);
			RowWidget->setObjectName(IsActive ? "WorkshopVersionRowActive" : "WorkshopVersionRow");
			QHBoxLayout* RowLayout = new QHBoxLayout(RowWidget);
			RowLayout->setContentsMargins(12, 12, 12, 12);
			RowLayout->setSpacing(12);

			QLabel* Thumb = new QLabel(RowWidget);
			Thumb->setFixedSize(64, 64);
			Thumb->setObjectName("BackgroundPreview");
			const QString ThumbPath = QDir::cleanPath(QString("%1/%2").arg(FolderPath, Root.value("Thumbnail").toString()));
			UpdateBackgroundPreviewLabel(Thumb, ThumbPath);
			RowLayout->addWidget(Thumb);

			QVBoxLayout* TextLayout = new QVBoxLayout();
			TextLayout->setContentsMargins(0, 0, 0, 0);
			TextLayout->setSpacing(4);
			const QString VersionDisplayLabel = Root.value("LauncherVersionLabel").toString().trimmed().isEmpty()
				? HumanizedVersionLabel(Folder)
				: Root.value("LauncherVersionLabel").toString().trimmed();
			QLabel* VersionName = new QLabel(QString("Version: %1").arg(VersionDisplayLabel), RowWidget);
			VersionName->setObjectName("WorkshopVersionTitleLabel");
			VersionName->setStyleSheet("font-weight:700;");
			TextLayout->addWidget(VersionName);
			QLabel* IdLabel = new QLabel(QString("Workshop ID: %1").arg(Root.value("PublisherID").toString().isEmpty() ? "none" : Root.value("PublisherID").toString()), RowWidget);
			IdLabel->setObjectName("WorkshopVersionIdLabel");
			TextLayout->addWidget(IdLabel);
			RowLayout->addLayout(TextLayout, 1);

			if (IsActive)
			{
				QLabel* ActiveLabel = new QLabel("ACTIVE", RowWidget);
				ActiveLabel->setObjectName("WorkshopVersionActiveBadge");
				RowLayout->addWidget(ActiveLabel, 0, Qt::AlignTop);
			}

			VersionsList->addItem(ListItem);
			VersionsList->setItemWidget(ListItem, RowWidget);
		}

		if (!ActiveFolderPath.isEmpty())
		{
			for (int Idx = 0; Idx < VersionsList->count(); Idx++)
			{
				QListWidgetItem* ListItem = VersionsList->item(Idx);
				if (ListItem && ListItem->data(Qt::UserRole).toString() == ActiveFolderPath)
				{
					VersionsList->setCurrentItem(ListItem);
					break;
				}
			}
		}
	};
	RefreshVersions();

	QHBoxLayout* Buttons = new QHBoxLayout();
	QPushButton* SaveCurrentButton = new QPushButton("Save Current");
	QPushButton* ImportButton = new QPushButton("Import workshop.json");
	QPushButton* ActivateButton = new QPushButton("Select Version");
	QPushButton* EditActiveButton = new QPushButton("Edit Active JSON");
	QPushButton* EditSelectedButton = new QPushButton("Edit Selected JSON");
	QPushButton* DeleteButton = new QPushButton("Delete Selected");
	QPushButton* OpenFolderButton = new QPushButton("Open Folder");
	Buttons->addWidget(SaveCurrentButton);
	Buttons->addWidget(ImportButton);
	Buttons->addWidget(ActivateButton);
	Buttons->addWidget(EditActiveButton);
	Buttons->addWidget(EditSelectedButton);
	Buttons->addWidget(DeleteButton);
	Buttons->addWidget(OpenFolderButton);
	Layout->addLayout(Buttons);

	connect(SaveCurrentButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		const QString CurrentJson = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		if (!QFileInfo(CurrentJson).isFile())
		{
			QMessageBox::information(&Dialog, "Workshop Versions", "No active workshop.json was found in the live zone folder.");
			return;
		}

		bool Accepted = false;
		const QString Label = QInputDialog::getText(&Dialog, "Save Current Version", "Version label:", QLineEdit::Normal, "", &Accepted).trimmed();
		if (!Accepted)
			return;

		if (SaveWorkshopVersionSnapshot(ItemType, ContainerName, EntryName, CurrentJson, Label))
			RefreshVersions();
	});

	connect(ImportButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		const QString ImportPath = QFileDialog::getOpenFileName(&Dialog, "Import workshop.json", QString(), "workshop.json (workshop.json);;JSON Files (*.json)");
		if (ImportPath.isEmpty())
			return;

		bool Accepted = false;
		const QString Label = QInputDialog::getText(&Dialog, "Import Workshop Version", "Version label:", QLineEdit::Normal, QFileInfo(ImportPath).baseName(), &Accepted).trimmed();
		if (!Accepted)
			return;

		const QString OverrideId = QInputDialog::getText(&Dialog, "Workshop ID", "Optional PublisherID override:", QLineEdit::Normal).trimmed();
		if (SaveWorkshopVersionSnapshot(ItemType, ContainerName, EntryName, ImportPath, Label, OverrideId))
			RefreshVersions();
	});

	connect(ActivateButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;

		if (ActivateWorkshopVersion(ItemType, ContainerName, EntryName, Current->data(Qt::UserRole).toString()))
		{
			RefreshVersions();
			Dialog.accept();
		}
		else
			QMessageBox::warning(&Dialog, "Workshop Versions", "Unable to activate the selected version.");
	});
	connect(EditActiveButton, &QPushButton::clicked, &Dialog, [&]()
	{
		const QString ActiveJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(ActiveFolder));
		if (EditJsonTextDialog(&Dialog, ActiveJsonPath, "Edit Active workshop.json"))
			RefreshVersions();
	});
	connect(EditSelectedButton, &QPushButton::clicked, &Dialog, [&]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;
		const QString SelectedJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(Current->data(Qt::UserRole).toString()));
		if (EditJsonTextDialog(&Dialog, SelectedJsonPath, "Edit Saved workshop.json"))
			RefreshVersions();
	});
	connect(VersionsList, &QListWidget::itemDoubleClicked, &Dialog, [&, this](QListWidgetItem* Current)
	{
		if (Current && ActivateWorkshopVersion(ItemType, ContainerName, EntryName, Current->data(Qt::UserRole).toString()))
		{
			RefreshVersions();
			Dialog.accept();
		}
	});

	connect(DeleteButton, &QPushButton::clicked, &Dialog, [&, this]()
	{
		QListWidgetItem* Current = VersionsList->currentItem();
		if (!Current)
			return;

		const QString FolderPath = Current->data(Qt::UserRole).toString();
		if (QMessageBox::warning(&Dialog, "Delete Workshop Version", QString("Delete workshop version '%1'?").arg(QFileInfo(FolderPath).fileName()), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		QDir(FolderPath).removeRecursively();
		RefreshVersions();
	});

	connect(OpenFolderButton, &QPushButton::clicked, &Dialog, [&]()
	{
		ShellExecute(NULL, "open", QString("\"%1\"").arg(QDir::toNativeSeparators(VersionsRoot)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	});

	QDialogButtonBox* CloseButtons = new QDialogButtonBox(QDialogButtonBox::Close, &Dialog);
	connect(CloseButtons, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(CloseButtons);
	Dialog.exec();
}

void mlMainWindow::UpdateDB()
{
	if (mBuildThread)
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));

	StartBuildThread(Commands);
}

void mlMainWindow::StartBuildThread(const QList<QPair<QString, QStringList>>& Commands)
{
	mOutputWidget->clear();
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	for (const QPair<QString, QStringList>& Command : Commands)
	{
		if (IsGameLaunchCommand(Command.first, Command.second) && mRunOnlineWidget && mRunOnlineWidget->isChecked())
		{
			QString TargetLabel = "selected content";
			const int FsGameIdx = Command.second.indexOf("fs_game");
			if (FsGameIdx >= 0 && FsGameIdx + 1 < Command.second.count())
				TargetLabel = QString("mod '%1'").arg(Command.second[FsGameIdx + 1]);
			const int DevmapIdx = Command.second.indexOf("+devmap");
			if (DevmapIdx >= 0 && DevmapIdx + 1 < Command.second.count())
				TargetLabel = QString("map '%1'").arg(Command.second[DevmapIdx + 1]);
			ShowOnlineLaunchProgressDialog(TargetLabel);
			break;
		}
	}

	mBuildThread = new mlBuildThread(Commands, mIgnoreErrorsWidget->isChecked());
	connect(mBuildThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mBuildThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mBuildThread->start();
}

void mlMainWindow::StartConvertThread(QStringList& pathList, QString& outputDir, bool allowOverwrite)
{
	if (mOutputWidget)
		mOutputWidget->clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mPendingOutput.clear();
	mOutputFullText.clear();
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	mConvertThread = new mlConvertThread(pathList, outputDir, true, allowOverwrite);
	connect(mConvertThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mConvertThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mConvertThread->start();
}

void mlMainWindow::SetActiveBuildButton(BuildLanguageMode Mode)
{
	ResetBuildButtons();
	mActiveBuildButton = (Mode == BuildAllLanguages) ? mBuildButton : mBuildAllLanguagesButton;
	mActiveBuildButton->setText("Cancel");
	((mActiveBuildButton == mBuildButton) ? mBuildAllLanguagesButton : mBuildButton)->setEnabled(false);
}

void mlMainWindow::ResetBuildButtons()
{
	mActiveBuildButton = NULL;
	UpdateBuildActionButtons();
}

void mlMainWindow::UpdateBuildActionButtons()
{
	if (!mBuildButton || !mBuildAllLanguagesButton)
		return;

	if (mBuildThread)
		return;

	const bool RunOnlyMode = mRunEnabledWidget && mRunEnabledWidget->isChecked()
		&& mCompileEnabledWidget && !mCompileEnabledWidget->isChecked()
		&& mLightEnabledWidget && !mLightEnabledWidget->isChecked()
		&& mLinkEnabledWidget && !mLinkEnabledWidget->isChecked();
	const bool HasCheckedTargets = !CollectTargetItems().isEmpty();
	QTreeWidgetItem* CurrentItem = mFileListWidget ? mFileListWidget->currentItem() : NULL;
	const bool HasRunnableSelection = CurrentItem && CurrentItem->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN;
	const bool HasEnabledAction = (mCompileEnabledWidget && mCompileEnabledWidget->isChecked())
		|| (mLightEnabledWidget && mLightEnabledWidget->isChecked())
		|| (mLinkEnabledWidget && mLinkEnabledWidget->isChecked())
		|| (mRunEnabledWidget && mRunEnabledWidget->isChecked());
	const bool CanBuild = HasCheckedTargets && HasEnabledAction;
	const bool CanRunSelectedOnly = RunOnlyMode && HasRunnableSelection;

	QString RunOnlyLabel = "Start Game";
	if (HasRunnableSelection)
	{
		const int ItemType = CurrentItem->data(0, Qt::UserRole).toInt();
		RunOnlyLabel = (ItemType == ML_ITEM_MAP) ? "Start Map" : "Start Mod";
	}

	mBuildButton->setText(RunOnlyMode ? RunOnlyLabel : "Build");
	mBuildButton->setEnabled(CanBuild || CanRunSelectedOnly);
	mBuildAllLanguagesButton->setText("Build (English)");
	mBuildAllLanguagesButton->setEnabled(CanBuild && !RunOnlyMode);
}

void mlMainWindow::ApplyLauncherLayout()
{
	if (!mCentralWidgetSplitter || !mTopWidget || !mTopLayout || !mLeftPanel || !mActionsPanel || !mOutputPanel)
		return;

	while (mTopLayout->count() > 0)
	{
		QLayoutItem* Item = mTopLayout->takeAt(0);
		delete Item;
	}

	mTopLayout->addWidget(mLeftPanel);
	mTopLayout->addWidget(mActionsPanel);
	mTopLayout->setStretch(0, 1);
	mTopLayout->setStretch(1, 0);

	while (mCentralWidgetSplitter->count() > 0)
	{
		QWidget* Widget = mCentralWidgetSplitter->widget(0);
		if (!Widget)
			break;
		Widget->hide();
		Widget->setParent(NULL);
	}

	const QString LayoutKey = mLauncherLayout.trimmed().toLower();
	if (LayoutKey == "left-build-console")
	{
		mCentralWidgetSplitter->setOrientation(Qt::Horizontal);
		mCentralWidgetSplitter->addWidget(mLeftPanel);
		mCentralWidgetSplitter->addWidget(mActionsPanel);
		mCentralWidgetSplitter->addWidget(mOutputPanel);
		mCentralWidgetSplitter->setSizes(QList<int>() << 430 << 240 << 520);
		mTopWidget->hide();
	}
	else if (LayoutKey == "left-console-build")
	{
		mCentralWidgetSplitter->setOrientation(Qt::Horizontal);
		mCentralWidgetSplitter->addWidget(mLeftPanel);
		mCentralWidgetSplitter->addWidget(mOutputPanel);
		mCentralWidgetSplitter->addWidget(mActionsPanel);
		mCentralWidgetSplitter->setSizes(QList<int>() << 430 << 520 << 240);
		mTopWidget->hide();
	}
	else
	{
		mCentralWidgetSplitter->setOrientation(Qt::Vertical);
		mCentralWidgetSplitter->addWidget(mTopWidget);
		mCentralWidgetSplitter->addWidget(mOutputPanel);
		mCentralWidgetSplitter->setSizes(QList<int>() << 290 << 450);
		mTopWidget->show();
	}

	mLeftPanel->show();
	mActionsPanel->show();
	mOutputPanel->show();
}

QString mlMainWindow::GetItemContainerName(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_CONTAINER_ROLE).toString() : QString();
}

QString mlMainWindow::GetItemEntryName(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_NAME_ROLE).toString() : QString();
}

QString mlMainWindow::GetItemFavoriteKey(QTreeWidgetItem* Item) const
{
	return Item ? Item->data(0, ML_ITEM_FAVORITE_ROLE).toString() : QString();
}

QStringList mlMainWindow::FavoriteEntries() const
{
	QSettings Settings;
	return Settings.value("Favorites", QStringList()).toStringList();
}

QStringList mlMainWindow::RecentEntries() const
{
	QSettings Settings;
	return Settings.value("Recents", QStringList()).toStringList();
}

bool mlMainWindow::IsFavoriteEntry(const QString& Entry) const
{
	return FavoriteEntries().contains(Entry, Qt::CaseInsensitive);
}

void mlMainWindow::ToggleFavoriteEntry(const QString& Entry)
{
	if (Entry.isEmpty())
		return;

	QSettings Settings;
	QStringList Entries = FavoriteEntries();
	auto ToggleSingleEntry = [&](const QString& Value)
	{
		const int Index = Entries.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(Value)), QRegularExpression::CaseInsensitiveOption));

		if (Index >= 0)
			Entries.removeAt(Index);
		else
			Entries.append(Value);
	};

	ToggleSingleEntry(Entry);
	QRegularExpression ModGroupExpression("^mod:([^/]+)$", QRegularExpression::CaseInsensitiveOption);
	QRegularExpressionMatch Match = ModGroupExpression.match(Entry);
	if (Match.hasMatch())
	{
		for (const QString& ZoneName : ModZoneNames(Match.captured(1)))
			ToggleSingleEntry(QString("mod:%1/%2").arg(Match.captured(1).toLower(), ZoneName.toLower()));
	}

	Settings.setValue("Favorites", Entries);
}

void mlMainWindow::TouchRecentEntry(const QString& Entry)
{
	if (Entry.isEmpty())
		return;

	QSettings Settings;
	QStringList Entries = RecentEntries();
	const int Index = Entries.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(Entry)), QRegularExpression::CaseInsensitiveOption));
	if (Index >= 0)
		Entries.removeAt(Index);
	Entries.prepend(Entry);
	while (Entries.count() > 12)
		Entries.removeLast();
	Settings.setValue("Recents", Entries);
}

QString mlMainWindow::RecentEntryForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MAP)
		return QString("map:%1").arg(EntryName.toLower());

	if (ItemType == ML_ITEM_MOD || ItemType == ML_ITEM_MOD_GROUP)
		return QString("mod:%1").arg(ContainerName.toLower());

	return QString();
}

QStringList mlMainWindow::ModZoneNames(const QString& ModName) const
{
	QStringList Zones;
	const char* Files[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };
	for (int FileIdx = 0; FileIdx < 4; FileIdx++)
	{
		QString ZoneFileName = QString("%1/mods/%2/zone_source/%3.zone").arg(mGamePath, ModName, Files[FileIdx]);
		if (QFileInfo(ZoneFileName).isFile())
			Zones << Files[FileIdx];
	}
	return Zones;
}

Qt::CheckState mlMainWindow::CheckStateForKey(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return Qt::Unchecked;

	return mCheckedStateByKey.value(FavoriteKey.toLower(), Qt::Unchecked);
}

static Qt::CheckState ItemCheckState(QTreeWidgetItem* Item)
{
	return Item ? static_cast<Qt::CheckState>(Item->data(0, ML_ITEM_CHECKSTATE_ROLE).toInt()) : Qt::Unchecked;
}

void mlMainWindow::ApplyCheckStateForKey(const QString& FavoriteKey, Qt::CheckState State, QTreeWidgetItem* SkipItem)
{
	if (FavoriteKey.isEmpty())
		return;

	const QString NormalizedKey = FavoriteKey.toLower();
	mCheckedStateByKey.insert(NormalizedKey, State);

	std::function<void(QTreeWidgetItem*)> ApplyState = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (Child != SkipItem && GetItemFavoriteKey(Child).compare(FavoriteKey, Qt::CaseInsensitive) == 0)
			{
				Child->setData(0, ML_ITEM_CHECKSTATE_ROLE, State);
				SyncItemCheckWidget(Child);
			}

			ApplyState(Child);
		}
	};

	ApplyState(mFileListWidget->invisibleRootItem());
}

void mlMainWindow::SyncItemCheckWidget(QTreeWidgetItem* Item) const
{
	if (!Item)
		return;

	QWidget* Widget = mFileListWidget ? mFileListWidget->itemWidget(Item, 0) : NULL;
	if (!Widget)
		return;

	QCheckBox* CheckBox = Widget->findChild<QCheckBox*>("ItemSelectCheckBox");
	if (!CheckBox)
		return;

	CheckBox->blockSignals(true);
	CheckBox->setChecked(ItemCheckState(Item) == Qt::Checked);
	CheckBox->blockSignals(false);
}

void mlMainWindow::UpdateParentCheckState(QTreeWidgetItem* Item)
{
	QTreeWidgetItem* Parent = Item ? Item->parent() : NULL;
	while (Parent)
	{
		int CheckedChildren = 0;
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			if (ItemCheckState(Parent->child(ChildIdx)) == Qt::Checked)
				CheckedChildren++;
		}

		const Qt::CheckState NewState = (CheckedChildren == Parent->childCount() && Parent->childCount() > 0) ? Qt::Checked : Qt::Unchecked;
		const QString ParentKey = GetItemFavoriteKey(Parent);
		if (!ParentKey.isEmpty())
			ApplyCheckStateForKey(ParentKey, NewState);
		else
		{
			Parent->setData(0, ML_ITEM_CHECKSTATE_ROLE, NewState);
			SyncItemCheckWidget(Parent);
		}
		Parent = Parent->parent();
	}
}

QString mlMainWindow::ResolveSteamExecutablePath() const
{
	QSettings SteamUser(QString("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
	const QString SteamExe = QDir::fromNativeSeparators(SteamUser.value("SteamExe").toString().trimmed());
	if (!SteamExe.isEmpty() && QFileInfo(SteamExe).exists())
		return SteamExe;

	QSettings SteamMachine(QString("HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Valve\\Steam"), QSettings::NativeFormat);
	const QString InstallPath = QDir::fromNativeSeparators(SteamMachine.value("InstallPath").toString().trimmed());
	if (!InstallPath.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(InstallPath + "/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	const QString ProgramFilesX86 = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("ProgramFiles(x86)")));
	if (!ProgramFilesX86.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(ProgramFilesX86 + "/Steam/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	const QString ProgramFiles = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("ProgramFiles")));
	if (!ProgramFiles.isEmpty())
	{
		const QString Candidate = QDir::cleanPath(ProgramFiles + "/Steam/Steam.exe");
		if (QFileInfo(Candidate).exists())
			return Candidate;
	}

	return QString("Steam.exe");
}

QPair<QString, QStringList> mlMainWindow::CreateGameLaunchCommand(const QString& FsGame, const QString& MapName) const
{
	QStringList Args;
	if (!mRunDvars.isEmpty())
		Args << mRunDvars;
	Args << "+set" << "fs_game" << FsGame;
	if (!MapName.isEmpty())
		Args << "+devmap" << MapName;

	const QString ExtraOptions = mRunOptionsWidget ? mRunOptionsWidget->text() : QString();
	if (!ExtraOptions.isEmpty())
		Args << ExtraOptions.split(' ', Qt::SkipEmptyParts);

	if (mRunOnlineWidget && mRunOnlineWidget->isChecked())
		return qMakePair(ResolveSteamExecutablePath(), QStringList() << "-applaunch" << QString::number(AppId) << Args);

	return qMakePair(QString("%1/BlackOps3.exe").arg(mGamePath), Args);
}

void mlMainWindow::SetTreeItemChecked(QTreeWidgetItem* Item, Qt::CheckState State, bool PropagateChildren)
{
	if (!Item)
		return;

	const QString FavoriteKey = GetItemFavoriteKey(Item);
	if (!FavoriteKey.isEmpty())
		ApplyCheckStateForKey(FavoriteKey, State, Item);

	Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, State);
	SyncItemCheckWidget(Item);

	if (PropagateChildren)
	{
		for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
			SetTreeItemChecked(Item->child(ChildIdx), State, true);
	}

	UpdateParentCheckState(Item);
	UpdateBuildActionButtons();
}

bool mlMainWindow::SupportsDisplayName(int ItemType) const
{
	return ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD_GROUP;
}

QString mlMainWindow::DisplayNameForEntry(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return QString();

	QSettings Settings;
	return Settings.value(QString("DisplayNames/%1").arg(FavoriteKey.toLower())).toString().trimmed();
}

void mlMainWindow::SetDisplayNameForEntry(const QString& FavoriteKey, const QString& DisplayName)
{
	if (FavoriteKey.isEmpty())
		return;

	QSettings Settings;
	const QString SettingKey = QString("DisplayNames/%1").arg(FavoriteKey.toLower());
	const QString TrimmedName = DisplayName.trimmed();
	if (TrimmedName.isEmpty())
		Settings.remove(SettingKey);
	else
		Settings.setValue(SettingKey, TrimmedName);
}

QString mlMainWindow::DisplayColorForEntry(const QString& FavoriteKey) const
{
	if (FavoriteKey.isEmpty())
		return QString();

	QSettings Settings;
	return NormalizedStoredColor(Settings.value(QString("DisplayColors/%1").arg(FavoriteKey.toLower())).toString());
}

void mlMainWindow::SetDisplayColorForEntry(const QString& FavoriteKey, const QString& ColorValue)
{
	if (FavoriteKey.isEmpty())
		return;

	QSettings Settings;
	const QString SettingKey = QString("DisplayColors/%1").arg(FavoriteKey.toLower());
	const QString NormalizedColor = NormalizedStoredColor(ColorValue);
	if (NormalizedColor.isEmpty())
		Settings.remove(SettingKey);
	else
		Settings.setValue(SettingKey, NormalizedColor);
}

bool mlMainWindow::PromptForDisplayName(int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey)
{
	if (!SupportsDisplayName(ItemType) || FavoriteKey.isEmpty())
		return false;

	const QString InternalName = DisplayTextForItem(ItemType, ContainerName, EntryName);
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Display Options");
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QLabel* Intro = new QLabel(QString("Customize how '%1' appears in the launcher list.").arg(InternalName), &Dialog);
	Intro->setWordWrap(true);
	Layout->addWidget(Intro);

	QFormLayout* FormLayout = new QFormLayout();
	Layout->addLayout(FormLayout);

	QLineEdit* DisplayNameEdit = new QLineEdit(DisplayNameForEntry(FavoriteKey), &Dialog);
	FormLayout->addRow("Display name:", DisplayNameEdit);

	QWidget* ColorWidget = new QWidget(&Dialog);
	QHBoxLayout* ColorLayout = new QHBoxLayout(ColorWidget);
	ColorLayout->setContentsMargins(0, 0, 0, 0);
	ColorLayout->setSpacing(6);
	QLineEdit* ColorEdit = new QLineEdit(DisplayColorForEntry(FavoriteKey), ColorWidget);
	ColorEdit->setPlaceholderText("#ff8a2a");
	QPushButton* ColorBrowseButton = new QPushButton(ColorWidget);
	ColorBrowseButton->setFixedSize(26, 26);
	ColorBrowseButton->setToolTip("Pick selection color");
	QToolButton* ColorHelpButton = new QToolButton(ColorWidget);
	ColorHelpButton->setText("?");
	ColorHelpButton->setToolTip("Colors only change how this map or mod appears inside the launcher list.");
	QPushButton* ClearColorButton = new QPushButton("Clear", ColorWidget);
	ColorLayout->addWidget(ColorEdit, 1);
	ColorLayout->addWidget(ColorBrowseButton);
	ColorLayout->addWidget(ColorHelpButton);
	ColorLayout->addWidget(ClearColorButton);
	FormLayout->addRow("Selection color:", ColorWidget);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	Layout->addWidget(ButtonBox);

	auto RefreshColorButton = [=]()
	{
		const QString NormalizedColor = NormalizedStoredColor(ColorEdit->text());
		const QString SafeColor = NormalizedColor.isEmpty() ? QString("#444444") : NormalizedColor;
		ColorBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	};

	connect(ColorBrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit]()
	{
		QColor Color = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, "Select Selection Color");
		if (Color.isValid())
			ColorEdit->setText(Color.name(QColor::HexRgb));
	});
	connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshColorButton(); });
	connect(ClearColorButton, &QPushButton::clicked, &Dialog, [=]() { ColorEdit->clear(); });
	connect(ColorHelpButton, &QToolButton::clicked, &Dialog, [&, this]()
	{
		QMessageBox::information(&Dialog, "Selection Color", "Use a custom color to make a favorite map or mod stand out in the launcher list. This only changes the launcher UI, not the in-game name.");
	});
	RefreshColorButton();

	if (Dialog.exec() != QDialog::Accepted)
		return false;

	SetDisplayNameForEntry(FavoriteKey, DisplayNameEdit->text().trimmed());
	SetDisplayColorForEntry(FavoriteKey, ColorEdit->text());
	return true;
}

QList<QTreeWidgetItem*> mlMainWindow::CollectTargetItems(bool* HasMapSelection) const
{
	QList<QTreeWidgetItem*> TargetItems;
	QSet<QString> TargetKeys;
	bool FoundMapSelection = false;

	std::function<void(QTreeWidgetItem*)> CollectChecked = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			{
				const QString FavoriteKey = GetItemFavoriteKey(Child);
				if (!TargetKeys.contains(FavoriteKey))
				{
					TargetItems.append(Child);
					TargetKeys.insert(FavoriteKey);
					FoundMapSelection |= (Child->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
				}
			}
			else
				CollectChecked(Child);
		}
	};
	CollectChecked(mFileListWidget->invisibleRootItem());

	if (TargetItems.isEmpty())
	{
		QTreeWidgetItem* CurrentItem = mFileListWidget->currentItem();
		if (!CurrentItem)
		{
			QList<QTreeWidgetItem*> SelectedItems = mFileListWidget->selectedItems();
			if (!SelectedItems.isEmpty())
				CurrentItem = SelectedItems[0];
		}

		if (CurrentItem && CurrentItem->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
		{
			TargetItems.append(CurrentItem);
			FoundMapSelection = (CurrentItem->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
		}
	}

	if (HasMapSelection)
		*HasMapSelection = FoundMapSelection;

	return TargetItems;
}

bool mlMainWindow::MapMatchesCurrentTab(const QString& MapName) const
{
	if (!mCategoryTabs)
		return true;

	switch (mCategoryTabs->currentIndex())
	{
	case CategoryZMMaps:
		return MapName.startsWith("zm_", Qt::CaseInsensitive);
	case CategoryMPMaps:
		return MapName.startsWith("mp_", Qt::CaseInsensitive);
	default:
		return true;
	}
}

QString mlMainWindow::DisplayTextForItem(int ItemType, const QString& ContainerName, const QString& EntryName) const
{
	if (ItemType == ML_ITEM_MOD)
	{
		QString ZoneDescription;
		if (EntryName.compare("core_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Lobby .zone file";
		else if (EntryName.compare("zm_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Zombie .zone file";
		else if (EntryName.compare("mp_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Multiplayer .zone file";
		else if (EntryName.compare("cp_mod", Qt::CaseInsensitive) == 0)
			ZoneDescription = "Campaign .zone file";

		return ZoneDescription.isEmpty() ? EntryName : QString("%1  (%2)").arg(EntryName, ZoneDescription);
	}
	return EntryName;
}

QWidget* mlMainWindow::CreateItemTitleWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, const QString& BaseText)
{
	const bool IsChildRow = Item && Item->parent() && Item->parent()->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN;
	const bool UseClassicChrome = ThemeUsesClassicChrome(mThemeMode);
	const bool UseUpdatedChrome = ThemeUsesUpdatedChrome(mThemeMode);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(mThemeMode);
	const int RowHeight = UseClassicChrome ? 22 : (UseUpdatedChrome ? 42 : 44);
	const int PanelHeight = UseClassicChrome ? 14 : (UseUpdatedChrome ? 32 : 34);
	const int VerticalMargin = UseClassicChrome ? 1 : (UseUpdatedChrome ? 3 : 4);
	const int ContentSpacing = UseClassicChrome ? 1 : 6;
	HoverRevealWidget* RowWidget = new HoverRevealWidget(mFileListWidget);
	RowWidget->setObjectName("ItemRowWidget");
	RowWidget->setAttribute(Qt::WA_StyledBackground, true);
	RowWidget->setFixedHeight(RowHeight);
	QHBoxLayout* RowLayout = new QHBoxLayout(RowWidget);
	RowLayout->setContentsMargins(0, VerticalMargin, 0, VerticalMargin);
	RowLayout->setSpacing(UseDarkModernChrome ? 4 : 0);

	HoverRevealWidget* Widget = new HoverRevealWidget(RowWidget);
	Widget->setObjectName("ItemTitleWidget");
	Widget->setAttribute(Qt::WA_StyledBackground, true);
	Widget->setProperty("childRow", IsChildRow);
	Widget->setProperty("selected", mFileListWidget && mFileListWidget->currentItem() == Item);
	Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	Widget->setFixedHeight(PanelHeight);
	QHBoxLayout* Layout = new QHBoxLayout(Widget);
	Layout->setContentsMargins(UseClassicChrome ? 1 : 9, UseClassicChrome ? 0 : 2, UseClassicChrome ? 1 : 7, UseClassicChrome ? 0 : 2);
	Layout->setSpacing(ContentSpacing);

	QCheckBox* CheckBox = new QCheckBox(Widget);
	CheckBox->setObjectName("ItemSelectCheckBox");
	CheckBox->setChecked(ItemCheckState(Item) == Qt::Checked);
	connect(CheckBox, &QCheckBox::toggled, this, [=](bool Checked)
	{
		SetTreeItemChecked(Item, Checked ? Qt::Checked : Qt::Unchecked, Item->childCount() > 0);
		mFileListWidget->setCurrentItem(Item);
	});
	Layout->addWidget(CheckBox, 0, Qt::AlignVCenter);
	if (UseClassicChrome)
		Layout->addSpacing(3);

	QPushButton* NameButton = new QPushButton(Widget);
	NameButton->setObjectName("ItemNameButton");
	NameButton->setFlat(true);
	NameButton->setCursor(Qt::PointingHandCursor);
	NameButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	NameButton->setText(SupportsDisplayName(ItemType) ? DisplayNameForEntry(FavoriteKey) : QString());
	if (NameButton->text().isEmpty())
		NameButton->setText(BaseText);
	const QString DisplayColor = SupportsDisplayName(ItemType) ? DisplayColorForEntry(FavoriteKey) : QString();
	if (!DisplayColor.isEmpty())
		NameButton->setStyleSheet(QString("color:%1; font-weight:700;").arg(DisplayColor));
	connect(NameButton, &QPushButton::clicked, this, [=]()
	{
		mFileListWidget->setCurrentItem(Item);
		if (PromptForDisplayName(ItemType, ContainerName, EntryName, FavoriteKey))
			PopulateFileList();
	});
	Layout->addWidget(NameButton, 0, Qt::AlignVCenter);

	if (SupportsDisplayName(ItemType) && !FavoriteKey.isEmpty() && DisplayNameForEntry(FavoriteKey).isEmpty())
	{
		QToolButton* DisplayNameButton = new QToolButton(Widget);
		DisplayNameButton->setObjectName("DisplayNameAddButton");
			DisplayNameButton->setText("+");
			DisplayNameButton->setToolTip("Add display name");
			DisplayNameButton->setCursor(Qt::PointingHandCursor);
			DisplayNameButton->setAutoRaise(false);
				DisplayNameButton->setFixedSize(UseClassicChrome ? 10 : 20, UseClassicChrome ? 10 : 20);
		connect(DisplayNameButton, &QToolButton::clicked, this, [=]()
		{
			mFileListWidget->setCurrentItem(Item);
			if (PromptForDisplayName(ItemType, ContainerName, EntryName, FavoriteKey))
				PopulateFileList();
		});
		Layout->addWidget(DisplayNameButton, 0, Qt::AlignVCenter);
		Widget->SetRevealWidget(DisplayNameButton);
	}

	if (SupportsDisplayName(ItemType))
	{
		QLabel* InternalNameLabel = new QLabel(BaseText, Widget);
		InternalNameLabel->setObjectName("ItemInternalNameLabel");
		InternalNameLabel->setVisible(!DisplayNameForEntry(FavoriteKey).isEmpty());
		Layout->addWidget(InternalNameLabel, 0, Qt::AlignVCenter);
	}

	const bool ShowItemTypeTags = QSettings().value("ShowItemTypeTags", true).toBool();
	if (ShowItemTypeTags && (ItemType == ML_ITEM_MAP || ItemType == ML_ITEM_MOD_GROUP))
	{
		QLabel* TypeTag = new QLabel(ItemType == ML_ITEM_MAP ? "Map" : "Mod", Widget);
		TypeTag->setObjectName("ItemTypeTag");
		TypeTag->setProperty("itemType", ItemType == ML_ITEM_MAP ? "map" : "mod");
		Layout->addWidget(TypeTag, 0, Qt::AlignVCenter);
	}
	Layout->addStretch(1);

	RowLayout->addWidget(Widget, 1);
	if (QWidget* ActionsWidget = CreateQuickActionsWidget(Item, ItemType, ContainerName, EntryName, FavoriteKey, IsChildRow))
		RowLayout->addWidget(ActionsWidget, 0);
	return RowWidget;
}

QWidget* mlMainWindow::CreateQuickActionsWidget(QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, bool IsChildRow)
{
	Q_UNUSED(FavoriteKey);

	if (ItemType == ML_ITEM_UNKNOWN)
		return NULL;

	const bool UseClassicChrome = ThemeUsesClassicChrome(mThemeMode);
	QWidget* Widget = new QWidget(mFileListWidget);
	Widget->setObjectName("QuickActionStrip");
	Widget->setAttribute(Qt::WA_StyledBackground, true);
	Widget->setProperty("childRow", IsChildRow);
	Widget->setProperty("selected", mFileListWidget && mFileListWidget->currentItem() == Item);
		Widget->setFixedHeight(UseClassicChrome ? 14 : 34);
	QHBoxLayout* Layout = new QHBoxLayout(Widget);
		Layout->setContentsMargins(UseClassicChrome ? 0 : 6, UseClassicChrome ? 0 : 2, UseClassicChrome ? 0 : 8, UseClassicChrome ? 0 : 2);
	Layout->setSpacing(1);

	auto MakeButton = [&](const QString& Text, const QString& ToolTip, QStyle::StandardPixmap IconType, bool LinkAction, bool RunAction) -> void
	{
			QPushButton* Button = new QPushButton(style()->standardIcon(IconType), Text, Widget);
			Button->setObjectName("QuickActionButton");
			Button->setToolTip(ToolTip);
			Button->setCursor(Qt::PointingHandCursor);
				Button->setMinimumSize(UseClassicChrome ? 30 : 54, UseClassicChrome ? 14 : 22);
			connect(Button, &QPushButton::clicked, this, [=]() { RunQuickAction(ItemType, ContainerName, EntryName, LinkAction, RunAction); });
			Layout->addWidget(Button);
		};

	MakeButton("Link", ItemType == ML_ITEM_MAP ? "Link map" : (ItemType == ML_ITEM_MOD_GROUP ? "Link full mod" : "Link zone"), QStyle::SP_ArrowForward, true, false);
	MakeButton("Run", ItemType == ML_ITEM_MAP ? "Run map" : "Run mod", QStyle::SP_MediaPlay, false, true);

	return Widget;
}

void mlMainWindow::SyncItemSelectionWidget(QTreeWidgetItem* Item) const
{
	if (!mFileListWidget || !Item)
		return;

	QWidget* RowWidget = mFileListWidget->itemWidget(Item, 0);
	if (!RowWidget)
		return;

	const bool Selected = (mFileListWidget->currentItem() == Item);
	auto ApplySelectionState = [Selected](QWidget* Widget)
	{
		if (!Widget)
			return;
		Widget->setProperty("selected", Selected);
		Widget->style()->unpolish(Widget);
		Widget->style()->polish(Widget);
		Widget->update();
	};

	ApplySelectionState(RowWidget->findChild<QWidget*>("ItemTitleWidget"));
	ApplySelectionState(RowWidget->findChild<QWidget*>("QuickActionStrip"));
}

void mlMainWindow::PopulatePinnedRoot(QTreeWidgetItem* RootItem, const QStringList& Keys, const QHash<QString, QVariantMap>& Lookup)
{
	QSet<QString> AddedEntries;
	const int PinnedRowHeight = TreeRowHeightForTheme(mThemeMode, true);
	auto AddPinnedItem = [&](QTreeWidgetItem* Parent, const QVariantMap& ItemData, const QString& Key, int Height) -> QTreeWidgetItem*
	{
		QTreeWidgetItem* Item = new QTreeWidgetItem(Parent, QStringList() << ItemData.value("display").toString());
		const int ItemType = ItemData.value("type").toInt();
		const QString ContainerName = ItemData.value("container").toString();
		const QString EntryName = ItemData.value("entry").toString();
		Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, CheckStateForKey(Key));
		Item->setData(0, Qt::UserRole, ItemType);
		Item->setData(0, ML_ITEM_CONTAINER_ROLE, ContainerName);
		Item->setData(0, ML_ITEM_NAME_ROLE, EntryName);
		Item->setData(0, ML_ITEM_FAVORITE_ROLE, Key);
		Item->setFlags(Item->flags() & ~Qt::ItemIsUserCheckable);
		Item->setForeground(0, QBrush(Qt::transparent));
		Item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
		Item->setSizeHint(0, QSize(0, Height));
		mFileListWidget->setItemWidget(Item, 0, CreateItemTitleWidget(Item, ItemType, ContainerName, EntryName, Key, ItemData.value("display").toString()));
		return Item;
	};

	for (const QString& Key : Keys)
	{
		const QString NormalizedKey = Key.toLower();
		if (AddedEntries.contains(NormalizedKey) || !Lookup.contains(Key))
			continue;

		const QVariantMap ItemData = Lookup.value(Key);
		if (ItemData.value("type").toInt() == ML_ITEM_MOD_GROUP)
		{
			QTreeWidgetItem* GroupItem = AddPinnedItem(RootItem, ItemData, Key, PinnedRowHeight);
			GroupItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
			AddedEntries.insert(NormalizedKey);

			for (const QString& ZoneName : ModZoneNames(ItemData.value("container").toString()))
			{
				const QString ChildKey = QString("mod:%1/%2").arg(ItemData.value("container").toString().toLower(), ZoneName.toLower());
				if (!Lookup.contains(ChildKey))
					continue;

				AddPinnedItem(GroupItem, Lookup.value(ChildKey), ChildKey, PinnedRowHeight);
				AddedEntries.insert(ChildKey.toLower());
			}

			GroupItem->setExpanded(true);
			continue;
		}

		AddPinnedItem(RootItem, ItemData, Key, PinnedRowHeight);
		AddedEntries.insert(NormalizedKey);
	}
}

void mlMainWindow::RunQuickAction(int ItemType, const QString& ContainerName, const QString& EntryName, bool LinkAction, bool RunAction)
{
	if (mBuildThread)
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));

	QStringList LanguageArgs;
	if (mBuildLanguage == "All")
	{
		for (const QString& Language : gLanguages)
			LanguageArgs << "-language" << Language;
	}
	else
		LanguageArgs << "-language" << mBuildLanguage;

	if (LinkAction)
	{
		if (ItemType == ML_ITEM_MAP)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << EntryName));
		}
		else if (ItemType == ML_ITEM_MOD)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ContainerName << "-modsource" << EntryName));
		}
		else if (ItemType == ML_ITEM_MOD_GROUP)
		{
			for (const QString& ZoneName : ModZoneNames(ContainerName))
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ContainerName << "-modsource" << ZoneName));
		}
	}

	if (RunAction)
	{
		Commands.append(CreateGameLaunchCommand(ItemType == ML_ITEM_MAP ? EntryName : ContainerName, ItemType == ML_ITEM_MAP ? EntryName : QString()));
	}

	if (Commands.isEmpty())
		return;

	TouchRecentEntry(RecentEntryForItem(ItemType, ContainerName, EntryName));
	if (LinkAction)
		SetActiveBuildButton(BuildAllLanguages);
	StartBuildThread(Commands);
}

QStringList mlMainWindow::DetectBuiltLanguages(const QString& ContentRoot) const
{
	QStringList BuiltLanguages;
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
	{
		const QString Language = gLanguages[LanguageIdx];
		if (QDir(QString("%1/%2").arg(ContentRoot, Language)).exists())
			BuiltLanguages << Language;
	}
	return BuiltLanguages;
}

bool mlMainWindow::HasOnlyEnglishBuild(const QString& ContentRoot) const
{
	QStringList BuiltLanguages = DetectBuiltLanguages(ContentRoot);
	BuiltLanguages.removeDuplicates();
	return !BuiltLanguages.isEmpty() && BuiltLanguages.count() == 1 && BuiltLanguages.contains("english");
}

void mlMainWindow::UpdateBackgroundOverlays()
{
	auto ApplyOverlay = [](QLabel* Overlay, QWidget* Viewport, const QString& CachePath)
	{
		if (!Overlay || !Viewport)
			return;

		Overlay->setGeometry(Viewport->rect());
		if (CachePath.isEmpty() || !QFileInfo(CachePath).isFile())
		{
			Overlay->clear();
			Overlay->hide();
			return;
		}

		const QPixmap Pixmap(CachePath);
		if (Pixmap.isNull())
		{
			Overlay->clear();
			Overlay->hide();
			return;
		}

		Overlay->setPixmap(Pixmap.scaled(Viewport->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
		Overlay->show();
		Overlay->lower();
	};

	ApplyOverlay(mAssetTreeBackgroundOverlay, mFileListWidget ? mFileListWidget->viewport() : NULL, mAssetTreeBackgroundCachePath);
	ApplyOverlay(mOutputBackgroundOverlay, mOutputWidget && mOutputWidget->isVisible() ? mOutputWidget->viewport() : NULL, mLogBackgroundCachePath);
}

void mlMainWindow::PopulateFileList()
{
	mFileListWidget->clear();
	QSettings Settings;
	const bool ShowRecents = Settings.value("ShowRecents", true).toBool();
	const bool ShowFavorites = Settings.value("ShowFavorites", true).toBool();
	const QStringList Favorites = FavoriteEntries();
	const QStringList Recents = RecentEntries();
	QHash<QString, QVariantMap> Lookup;
	const int ActiveTab = mCategoryTabs ? mCategoryTabs->currentIndex() : CategoryZMMaps;
	const int StandardRowHeight = TreeRowHeightForTheme(mThemeMode, false);
	QTreeWidgetItem* RecentsRootItem = NULL;
	QTreeWidgetItem* FavoritesRootItem = NULL;
	QTreeWidgetItem* MapsRootItem = NULL;
	QTreeWidgetItem* ModsRootItem = NULL;

	auto ConfigureItem = [&](QTreeWidgetItem* Item, int ItemType, const QString& ContainerName, const QString& EntryName, const QString& FavoriteKey, const QString& DisplayText, int Height)
	{
		Item->setText(0, DisplayText);
		Item->setData(0, ML_ITEM_CHECKSTATE_ROLE, CheckStateForKey(FavoriteKey));
		Item->setData(0, Qt::UserRole, ItemType);
		Item->setData(0, ML_ITEM_CONTAINER_ROLE, ContainerName);
		Item->setData(0, ML_ITEM_NAME_ROLE, EntryName);
		Item->setData(0, ML_ITEM_FAVORITE_ROLE, FavoriteKey);
		Item->setFlags(Item->flags() & ~Qt::ItemIsUserCheckable);
		Item->setForeground(0, QBrush(Qt::transparent));
		Item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
		Item->setSizeHint(0, QSize(0, Height));
		if (ItemType != ML_ITEM_UNKNOWN)
			Lookup.insert(FavoriteKey, QVariantMap{ { "type", ItemType }, { "container", ContainerName }, { "entry", EntryName }, { "display", DisplayText } });
		mFileListWidget->setItemWidget(Item, 0, CreateItemTitleWidget(Item, ItemType, ContainerName, EntryName, FavoriteKey, DisplayText));
	};

	QString UserMapsFolder = QDir::cleanPath(QString("%1/usermaps/").arg(mGamePath));
	QStringList UserMaps = QDir(UserMapsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	if (ActiveTab == CategoryAll)
	{
		RecentsRootItem = ShowRecents ? new QTreeWidgetItem(mFileListWidget, QStringList() << "Recents") : NULL;
		FavoritesRootItem = ShowFavorites ? new QTreeWidgetItem(mFileListWidget, QStringList() << "Favorites") : NULL;
		MapsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Maps");
		ModsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Mods");

		QFont Font = MapsRootItem->font(0);
		Font.setBold(true);
		if (RecentsRootItem)
		{
			RecentsRootItem->setFont(0, Font);
			RecentsRootItem->setFirstColumnSpanned(true);
			RecentsRootItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		}
		if (FavoritesRootItem)
		{
			FavoritesRootItem->setFont(0, Font);
			FavoritesRootItem->setFirstColumnSpanned(true);
			FavoritesRootItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		}
		MapsRootItem->setFont(0, Font);
		MapsRootItem->setFirstColumnSpanned(true);
		MapsRootItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		ModsRootItem->setFont(0, Font);
		ModsRootItem->setFirstColumnSpanned(true);
		ModsRootItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
	}

	for (QString MapName : UserMaps)
	{
		QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(UserMapsFolder, MapName, MapName);
		QString FavoriteKey = QString("map:%1").arg(MapName.toLower());

		if (QFileInfo(ZoneFileName).isFile())
		{
			Lookup.insert(FavoriteKey, QVariantMap{ { "type", ML_ITEM_MAP }, { "container", MapName }, { "entry", MapName }, { "display", MapName } });
			if (ActiveTab == CategoryAll)
			{
				QTreeWidgetItem* MapItem = new QTreeWidgetItem(MapsRootItem, QStringList() << MapName);
				ConfigureItem(MapItem, ML_ITEM_MAP, MapName, MapName, FavoriteKey, MapName, StandardRowHeight);
			}
			else if (ActiveTab == CategoryZMMaps || ActiveTab == CategoryMPMaps)
			{
				if (!MapMatchesCurrentTab(MapName))
					continue;
				QTreeWidgetItem* MapItem = new QTreeWidgetItem(mFileListWidget, QStringList() << MapName);
				ConfigureItem(MapItem, ML_ITEM_MAP, MapName, MapName, FavoriteKey, MapName, StandardRowHeight);
			}
		}
	}

	QString ModsFolder = QDir::cleanPath(QString("%1/mods/").arg(mGamePath));
	QStringList Mods = QDir(ModsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	const char* Files[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };

	for (QString ModName : Mods)
	{
		QTreeWidgetItem* ParentItem = NULL;
		const QString GroupFavoriteKey = QString("mod:%1").arg(ModName.toLower());
		Lookup.insert(GroupFavoriteKey, QVariantMap{ { "type", ML_ITEM_MOD_GROUP }, { "container", ModName }, { "entry", ModName }, { "display", ModName } });

		for (int FileIdx = 0; FileIdx < 4; FileIdx++)
		{
			QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(ModsFolder, ModName, Files[FileIdx]);

			if (QFileInfo(ZoneFileName).isFile())
			{
				QString FavoriteKey = QString("mod:%1/%2").arg(ModName.toLower(), QString(Files[FileIdx]).toLower());
				Lookup.insert(FavoriteKey, QVariantMap{ { "type", ML_ITEM_MOD }, { "container", ModName }, { "entry", QString(Files[FileIdx]) }, { "display", QString(Files[FileIdx]) } });

				if (ActiveTab != CategoryMods && ActiveTab != CategoryAll)
					continue;

				if (!ParentItem)
				{
					if (ActiveTab == CategoryAll)
						ParentItem = new QTreeWidgetItem(ModsRootItem, QStringList() << ModName);
					else
						ParentItem = new QTreeWidgetItem(mFileListWidget, QStringList() << ModName);
					ConfigureItem(ParentItem, ML_ITEM_MOD_GROUP, ModName, ModName, GroupFavoriteKey, ModName, StandardRowHeight);
					ParentItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
					ParentItem->setExpanded(true);
				}

				QTreeWidgetItem* ModItem = new QTreeWidgetItem(ParentItem, QStringList() << Files[FileIdx]);
				ConfigureItem(ModItem, ML_ITEM_MOD, ModName, Files[FileIdx], FavoriteKey, QString(Files[FileIdx]), StandardRowHeight);
			}
		}
	}

	if (ActiveTab == CategoryRecent)
		PopulatePinnedRoot(mFileListWidget->invisibleRootItem(), Recents, Lookup);
	else if (ActiveTab == CategoryFavorites)
		PopulatePinnedRoot(mFileListWidget->invisibleRootItem(), Favorites, Lookup);
	else if (ActiveTab == CategoryAll)
	{
		if (RecentsRootItem)
			PopulatePinnedRoot(RecentsRootItem, Recents, Lookup);
		if (FavoritesRootItem)
			PopulatePinnedRoot(FavoritesRootItem, Favorites, Lookup);
		MapsRootItem->sortChildren(0, Qt::AscendingOrder);
		ModsRootItem->sortChildren(0, Qt::AscendingOrder);
		if (RecentsRootItem)
			RecentsRootItem->setExpanded(Settings.value(SectionSettingKey("Recents") + "/Expanded", true).toBool());
		if (FavoritesRootItem)
			FavoritesRootItem->setExpanded(Settings.value(SectionSettingKey("Favorites") + "/Expanded", true).toBool());
		MapsRootItem->setExpanded(true);
		ModsRootItem->setExpanded(true);
	}

	if (ActiveTab != CategoryAll)
		mFileListWidget->sortItems(0, Qt::AscendingOrder);

	UpdateBuildActionButtons();
}

void mlMainWindow::OnEditReadyForPublish()
{
	if (mBuildThread)
		return;

	bool HasMapSelection = false;
	QList<QTreeWidgetItem*> CheckedItems = CollectTargetItems(&HasMapSelection);
	CheckedItems.clear();
	std::function<void(QTreeWidgetItem*)> CollectCheckedOnly = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
			{
				CheckedItems.append(Child);
				HasMapSelection |= (Child->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP);
			}
			CollectCheckedOnly(Child);
		}
	};
	CollectCheckedOnly(mFileListWidget->invisibleRootItem());
	if (CheckedItems.isEmpty())
	{
		QMessageBox::critical(this, "Ready for Publish", "No items are checked. Check at least one map or mod before using Ready for Publish.");
		return;
	}

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Ready for Publish");
	Dialog.resize(860, 320);
	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* Intro = new QLabel("Prepare checked maps/mods before publishing. This pass is best used right before a Workshop upload.");
	Intro->setWordWrap(true);
	Layout->addWidget(Intro);

	QLabel* Checklist = new QLabel("Recommended before upload:\n- Make sure your thumbnail follows the proper guidelines, check the Upload and click the ? next to the thumbnail");
	Checklist->setWordWrap(true);
	Checklist->setObjectName("InfoBanner");
	Layout->addWidget(Checklist);

	QCheckBox* RemoveXPaks = new QCheckBox("Remove XPaks");
	RemoveXPaks->setChecked(true);
	RemoveXPaks->setToolTip("Deletes generated .xpak files under the checked map/mod folders.");
	Layout->addWidget(RemoveXPaks);

	QCheckBox* CompileAllLanguages = new QCheckBox("Compile all languages");
	CompileAllLanguages->setChecked(true);
	CompileAllLanguages->setToolTip("Links the checked content for every supported language.");
	Layout->addWidget(CompileAllLanguages);

	QCheckBox* BuildLights = NULL;
	if (HasMapSelection)
	{
		BuildLights = new QCheckBox("Build lights");
		BuildLights->setChecked(true);
		BuildLights->setToolTip("Runs the lighting pass for checked maps before linking.");
		Layout->addWidget(BuildLights);
	}

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
	Layout->addWidget(ButtonBox);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	if (RemoveXPaks->isChecked())
	{
		for (QTreeWidgetItem* Item : CheckedItems)
		{
			const QString Folder = (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
				? QString("%1/usermaps/%2").arg(mGamePath, GetItemEntryName(Item))
				: QString("%1/mods/%2").arg(mGamePath, GetItemContainerName(Item));
			QDirIterator It(Folder, QStringList() << "*.xpak", QDir::Files, QDirIterator::Subdirectories);
			while (It.hasNext())
				QFile(It.next()).remove();
		}
	}

	if (!CompileAllLanguages->isChecked())
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));
	QStringList LanguageArgs;
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
		LanguageArgs << "-language" << gLanguages[LanguageIdx];

	for (QTreeWidgetItem* Item : CheckedItems)
	{
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), GetItemContainerName(Item), GetItemEntryName(Item)));

		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		{
			const QString MapName = GetItemEntryName(Item);
			QStringList CompileArgs;
			CompileArgs << "-platform" << "pc" << "-navmesh" << "-navvolume";
			CompileArgs << "-loadFrom" << QString("%1\\map_source\\%2\\%3.map").arg(mGamePath, MapName.left(2), MapName);
			CompileArgs << QString("%1\\share\\raw\\maps\\%2\\%3.d3dbsp").arg(mGamePath, MapName.left(2), MapName);
			Commands.append(QPair<QString, QStringList>(QString("%1\\bin\\cod2map64.exe").arg(mToolsPath), CompileArgs));

			if (BuildLights && BuildLights->isChecked())
			{
				QStringList LightArgs;
				LightArgs << "-ledSilent";
				switch (mLightQualityWidget->currentIndex())
				{
				case 0:
					LightArgs << "+low";
					break;
				default:
					LightArgs << "+medium";
					break;
				case 2:
					LightArgs << "+high";
					break;
				}
				LightArgs << "+localprobes" << "+forceclean" << "+recompute" << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), LightArgs));
			}

			Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << MapName));
		}
		else
		{
			const QString ModName = GetItemContainerName(Item);
			if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MOD_GROUP)
			{
				for (const QString& ZoneName : ModZoneNames(ModName))
					Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
			}
			else
			{
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << GetItemEntryName(Item)));
			}
		}
	}

	SetActiveBuildButton(BuildAllLanguages);
	StartBuildThread(Commands);
}

void mlMainWindow::ContextMenuRequested()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	const int ItemTypeId = Item->data(0, Qt::UserRole).toInt();
	QString ItemType = (ItemTypeId == ML_ITEM_MAP) ? "Map" : "Mod";

	if (ItemTypeId == ML_ITEM_UNKNOWN)
		return;

	QIcon GameIcon(":/resources/BlackOps3.png");

	QMenu* Menu = new QMenu;
	Menu->setStyleSheet(
		"QMenu { padding: 6px; }"
		"#DangerMenuSection { padding-top: 4px; }"
		"#DangerMenuButton {"
		" background: transparent; color: #ff7b7b; border: 0;"
		" padding: 4px 8px; text-align: left; font-weight: 700; }"
		"#DangerMenuButton:hover { background: #4a2020; border: 0; color: #ff9f66; }"
		"#DangerMenuButton:pressed { background: #351616; border: 0; color: #ffb0b0; }");
	Menu->addAction(GameIcon, QString("Run %1").arg(ItemType), this, SLOT(OnRunMapOrMod()));

	if (ItemTypeId == ML_ITEM_MAP)
		Menu->addAction(mActionFileLevelEditor);

	if (ItemTypeId == ML_ITEM_MAP || ItemTypeId == ML_ITEM_MOD)
		Menu->addAction("Edit Zone File", this, SLOT(OnOpenZoneFile()));
	QAction* PublishAction = Menu->addAction(QString("Publish %1").arg(ItemType));
	connect(PublishAction, &QAction::triggered, this, [=]()
	{
		std::function<void(QTreeWidgetItem*)> ClearCheckedItems = [&](QTreeWidgetItem* Parent)
		{
			for (int ChildIdx = 0; ChildIdx < Parent->childCount(); ChildIdx++)
			{
				QTreeWidgetItem* Child = Parent->child(ChildIdx);
				if (Child != Item && Child->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN && ItemCheckState(Child) == Qt::Checked)
					SetTreeItemChecked(Child, Qt::Unchecked, Child->childCount() > 0);
				ClearCheckedItems(Child);
			}
		};
		ClearCheckedItems(mFileListWidget->invisibleRootItem());
		SetTreeItemChecked(Item, Qt::Checked, Item->childCount() > 0);
		mFileListWidget->setCurrentItem(Item);
		OnEditPublish();
	});
	Menu->addAction(QString("Open %1 Folder").arg(ItemType), this, SLOT(OnOpenModRootFolder()));
	Menu->addAction(IsFavoriteEntry(GetItemFavoriteKey(Item)) ? "Remove Favorite" : "Add Favorite", this, SLOT(OnToggleFavorite()));
	QAction* NotesAction = Menu->addAction("Notes...");
	connect(NotesAction, &QAction::triggered, this, [=]() { EditNotesForItem(Item); });
	QAction* EditWorkshopJsonAction = Menu->addAction("Edit workshop.json...");
	connect(EditWorkshopJsonAction, &QAction::triggered, this, [=]()
	{
		const QString ItemFolder = (ItemTypeId == ML_ITEM_MAP)
			? QString("%1/usermaps/%2/zone").arg(mGamePath, GetItemEntryName(Item))
			: QString("%1/mods/%2/zone").arg(mGamePath, GetItemContainerName(Item));
		EditJsonTextDialog(this, QDir::cleanPath(QString("%1/workshop.json").arg(ItemFolder)), "Edit workshop.json");
	});
	QAction* WorkshopVersionsAction = Menu->addAction("Workshop Versions...");
	connect(WorkshopVersionsAction, &QAction::triggered, this, [=]() { ShowWorkshopVersionsDialog(Item); });
	Menu->addAction("Clean XPaks", this, SLOT(OnCleanXPaks()));
	Menu->addSeparator();

	QWidgetAction* DeleteAction = new QWidgetAction(Menu);
	QWidget* DeleteWidget = new QWidget(Menu);
	DeleteWidget->setObjectName("DangerMenuSection");
	DeleteWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	QHBoxLayout* DeleteLayout = new QHBoxLayout(DeleteWidget);
	DeleteLayout->setContentsMargins(0, 0, 0, 0);
	DeleteLayout->setSpacing(0);
	QPushButton* DeleteButton = new QPushButton(DeleteWidget);
	DeleteButton->setFlat(true);
	DeleteButton->setIcon(Menu->style()->standardIcon(QStyle::SP_MessageBoxWarning));
	DeleteButton->setIconSize(QSize(13, 13));
	DeleteButton->setText(QString("Delete %1").arg(ItemType));
	DeleteButton->setObjectName("DangerMenuButton");
	DeleteButton->setCursor(Qt::PointingHandCursor);
	DeleteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	DeleteButton->setMinimumHeight(24);
	DeleteLayout->addWidget(DeleteButton);
	connect(DeleteButton, &QPushButton::clicked, Menu, [=]()
	{
		Menu->close();
		OnDelete();
	});
	DeleteAction->setDefaultWidget(DeleteWidget);
	Menu->addAction(DeleteAction);

	Menu->exec(QCursor::pos());
}

void mlMainWindow::OnFileAssetEditor()
{
	QProcess* Process = new QProcess();
	connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
	Process->start(QString("%1/bin/AssetEditor_modtools.exe").arg(mToolsPath), QStringList());
}

void mlMainWindow::OnFileLevelEditor()
{
	QTreeWidgetItem* CheckedMap = NULL;
	std::function<void(QTreeWidgetItem*)> FindCheckedMap = [&](QTreeWidgetItem* Parent)
	{
		for (int ChildIdx = 0; ChildIdx < Parent->childCount() && !CheckedMap; ChildIdx++)
		{
			QTreeWidgetItem* Child = Parent->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked && Child->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
			{
				CheckedMap = Child;
				return;
			}
			FindCheckedMap(Child);
		}
	};
	FindCheckedMap(mFileListWidget->invisibleRootItem());

	if (CheckedMap)
	{
		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		QString MapName = GetItemEntryName(CheckedMap);
		TouchRecentEntry(GetItemFavoriteKey(CheckedMap));
		Process->start(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), QStringList() << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName));
		return;
	}

	QMessageBox::warning(this, "Open in Radiant", "Check a map first, then try Open in Radiant again.");
}

void mlMainWindow::OnFileExport2Bin()
{
	if (mExport2BinGUIWidget == NULL)
	{
		InitExport2BinGUI();
		mExport2BinGUIWidget->hide(); // Ensure the window is hidden (just in case)
	}

	mExport2BinGUIWidget->isVisible() ? mExport2BinGUIWidget->hide() : mExport2BinGUIWidget->show();
}

void mlMainWindow::OnFileNew()
{
	QDir TemplatesFolder(QString("%1/rex/templates").arg(mToolsPath));
	QStringList Templates = TemplatesFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	if (Templates.isEmpty())
	{
		QMessageBox::information(this, "Error", "Could not find any map templates.");
		return;
	}

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("New Map or Mod");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QFormLayout* FormLayout = new QFormLayout();
	Layout->addLayout(FormLayout);

	QLineEdit* NameWidget = new QLineEdit();
	NameWidget->setValidator(new QRegularExpressionValidator(QRegularExpression("[a-zA-Z0-9_]*"), this));
	FormLayout->addRow("Name:", NameWidget);

	QComboBox* TemplateWidget = new QComboBox();
	TemplateWidget->addItems(Templates);
	FormLayout->addRow("Template:", TemplateWidget);

	QFrame* Frame = new QFrame();
	Frame->setFrameShape(QFrame::HLine);
	Frame->setFrameShadow(QFrame::Raised);
	Layout->addWidget(Frame);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	QString Name = NameWidget->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, "Error", "Map name cannot be empty.");
		return;
	}

	if (mShippedMapList.contains(Name, Qt::CaseInsensitive))
	{
		QMessageBox::information(this, "Error", "Map name cannot be the same as a built-in map.");
		return;
	}

	QByteArray MapName = NameWidget->text().toLatin1().toLower();
	QString Output;

	QString Template = Templates[TemplateWidget->currentIndex()];

	if ((Template == "MP Mod Level" && !MapName.startsWith("mp_")) || (Template == "ZM Mod Level" && !MapName.startsWith("zm_")))
	{
		QMessageBox::information(this, "Error", "Map name must start with 'mp_' or 'zm_'.");
		return;
	}

	std::function<bool(const QString&, const QString&)> RecursiveCopy=[&](const QString& SourcePath, const QString& DestPath) -> bool
	{
		QDir Dir(SourcePath);
		if (!Dir.exists())
			return false;

		foreach (QString DirEntry, Dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		{
			QString NewPath = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			Dir.mkpath(NewPath);
			if (!RecursiveCopy(SourcePath + QDir::separator() + DirEntry, NewPath))
				return false;
		}

		foreach (QString DirEntry, Dir.entryList(QDir::Files))
		{
			QFile SourceFile(SourcePath + QDir::separator() + DirEntry);
			QString DestFileName = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			QFile DestFile(DestFileName);

			if (!SourceFile.open(QFile::ReadOnly) || !DestFile.open(QFile::WriteOnly))
				return false;

			while (!SourceFile.atEnd())
			{
				QByteArray Line = SourceFile.readLine();

				if (Line.contains("guid"))
				{
					QString LineString(Line);
					LineString.replace(QRegularExpression("guid \"\\{(.*)\\}\""), QString("guid \"%1\"").arg(QUuid::createUuid().toString()));
					Line = LineString.toLatin1();
				}
				else
					Line.replace("template", MapName);

				DestFile.write(Line);
			}

			Output += DestFileName + "\n";
		}

		return true;
	};

	if (RecursiveCopy(TemplatesFolder.absolutePath() + QDir::separator() + Templates[TemplateWidget->currentIndex()], QDir::cleanPath(mGamePath)))
	{
		PopulateFileList();

		QMessageBox::information(this, "New Map Created", QString("Files created:\n") + Output);
	}
	else
		QMessageBox::information(this, "Error", "Error creating map files.");
}

void mlMainWindow::OnEditBuild()
{
	StartBuild(BuildAllLanguages);
}

void mlMainWindow::OnEditBuildAllLanguages()
{
	StartBuild(BuildEnglishOnly);
}

void mlMainWindow::OnSetModernTheme()
{
	ApplyThemeProfile("original-updated");
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::OnSetDarkModernTheme()
{
	ApplyThemeProfile("dark-modern");
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::OnSetClassicTheme()
{
	ApplyThemeProfile("original-classic");
	UpdateTheme();
	PopulateFileList();
}
void mlMainWindow::UpdateThemeMenuChecks()
{
	if (mActionThemeModern)
	{
		mActionThemeModern->blockSignals(true);
		mActionThemeModern->setChecked(mThemeMode == ThemeOriginalUpdated);
		mActionThemeModern->blockSignals(false);
	}
	if (mActionThemeDarkModern)
	{
		mActionThemeDarkModern->blockSignals(true);
		mActionThemeDarkModern->setChecked(mThemeMode == ThemeDarkModern);
		mActionThemeDarkModern->blockSignals(false);
	}
	if (mActionThemeClassic)
	{
		mActionThemeClassic->blockSignals(true);
		mActionThemeClassic->setChecked(mThemeMode == ThemeOriginalClassic);
		mActionThemeClassic->blockSignals(false);
	}
}

void mlMainWindow::StartBuild(BuildLanguageMode Mode)
{
	QPushButton* TriggerButton = (Mode == BuildAllLanguages) ? mBuildButton : mBuildAllLanguagesButton;
	const bool RunOnlyMode = mRunEnabledWidget && mRunEnabledWidget->isChecked()
		&& mCompileEnabledWidget && !mCompileEnabledWidget->isChecked()
		&& mLightEnabledWidget && !mLightEnabledWidget->isChecked()
		&& mLinkEnabledWidget && !mLinkEnabledWidget->isChecked();

	if (RunOnlyMode && Mode == BuildEnglishOnly)
		return;

	if (mBuildThread)
	{
		if (mActiveBuildButton == TriggerButton)
			mBuildThread->Cancel();
		return;
	}

	QList<QPair<QString, QStringList>> Commands;
	bool UpdateAdded = false;
	auto AddUpdateDBCommand = [&]()
	{
		if (!UpdateAdded)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));
			UpdateAdded = true;
		}
	};

	QList<QTreeWidgetItem*> CheckedItems = CollectTargetItems();
	if (CheckedItems.isEmpty() && RunOnlyMode && mFileListWidget && mFileListWidget->currentItem()
		&& mFileListWidget->currentItem()->data(0, Qt::UserRole).toInt() != ML_ITEM_UNKNOWN)
	{
		CheckedItems.append(mFileListWidget->currentItem());
	}
	if (CheckedItems.isEmpty())
	{
		QMessageBox::warning(this, "Build", "Please select at least one file from the list.");
		return;
	}
	QString LastMap, LastMod;

	QStringList LanguageArgs;

	if (Mode == BuildEnglishOnly)
	{
		LanguageArgs << "-language" << "english";
	}
	else if (Mode == BuildAllLanguages || mBuildLanguage == "All")
	{
		for (const QString& Language : gLanguages)
			LanguageArgs << "-language" << Language;
	}
	else
		LanguageArgs << "-language" << mBuildLanguage;

	for (QTreeWidgetItem* Item : CheckedItems)
	{
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), GetItemContainerName(Item), GetItemEntryName(Item)));

		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		{
			QString MapName = GetItemEntryName(Item);

			if (mCompileEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-platform" << "pc";

				if (mCompileModeWidget->currentIndex() == 0)
					Args << "-onlyents";
				else
					Args << "-navmesh" << "-navvolume";

				Args << "-loadFrom" << QString("%1\\map_source\\%2\\%3.map").arg(mGamePath, MapName.left(2), MapName);
				Args << QString("%1\\share\\raw\\maps\\%2\\%3.d3dbsp").arg(mGamePath, MapName.left(2), MapName);

				Commands.append(QPair<QString, QStringList>(QString("%1\\bin\\cod2map64.exe").arg(mToolsPath), Args));
			}

			if (mLightEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-ledSilent";

				switch (mLightQualityWidget->currentIndex())
				{
				case 0:
					Args << "+low";
					break;
				default:
					Args << "+medium";
					break;

				case 2:
					Args << "+high";
					break;
				}

				Args << "+localprobes" << "+forceclean" << "+recompute" << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), Args));
			}

			if (mLinkEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << MapName));
			}

			LastMap = MapName;
		}
		else
		{
			QString ModName = GetItemContainerName(Item);
			const int ItemType = Item->data(0, Qt::UserRole).toInt();

			if (mLinkEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				if (ItemType == ML_ITEM_MOD_GROUP)
				{
					for (const QString& ZoneName : ModZoneNames(ModName))
						Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
				}
				else
				{
					QString ZoneName = GetItemEntryName(Item);
					Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
				}
			}

			LastMod = ModName;
		}
	}

	if (mRunEnabledWidget->isChecked() && (!LastMod.isEmpty() || !LastMap.isEmpty()))
	{
		Commands.append(CreateGameLaunchCommand(LastMod.isEmpty() ? LastMap : LastMod, LastMap));
	}

	if (Commands.size() == 0 && !UpdateAdded)
	{
		QMessageBox::information(this, "No Tasks", "Please selected at least one file from the list and one action to be performed.");
		return;
	}

	SetActiveBuildButton(Mode);
	StartBuildThread(Commands);
}

void mlMainWindow::OnEditPublish()
{
	std::function<QTreeWidgetItem* (QTreeWidgetItem*)> SearchCheckedItem=[&](QTreeWidgetItem* ParentItem) -> QTreeWidgetItem*
	{
		for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = ParentItem->child(ChildIdx);
			if (ItemCheckState(Child) == Qt::Checked)
				return Child;

			QTreeWidgetItem* Checked = SearchCheckedItem(Child);
			if (Checked)
				return Checked;
		}

		return nullptr;
	};

	QTreeWidgetItem* Item = SearchCheckedItem(mFileListWidget->invisibleRootItem());
	if (!Item)
	{
		QMessageBox::warning(this, "Error", "No maps or mods checked.");
		return;
	}

	QString Folder;
	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		Folder = "usermaps/" + GetItemEntryName(Item);
		mType = "map";
		mFolderName = GetItemEntryName(Item);
	}
	else
	{
		Folder = "mods/" + GetItemContainerName(Item);
		mType = "mod";
		mFolderName = GetItemContainerName(Item);
	}

	mWorkshopFolder = QString("%1/%2/zone").arg(mGamePath, Folder);
	QFile File(mWorkshopFolder + "/workshop.json");

	if (!QFileInfo(mWorkshopFolder).isDir())
	{
		QMessageBox::information(this, "Error", QString("The folder '%1' does not exist.").arg(mWorkshopFolder));
		return;
	}

	mFileId = 0;
	mTitle.clear();
	mBriefingDescription.clear();
	mSteamDescription.clear();
	mThumbnail.clear();
	mTags.clear();
	mPostUploadSteamSyncPending = false;

	if (File.open(QIODevice::ReadOnly))
	{
		QJsonDocument Document = QJsonDocument::fromJson(File.readAll());
		QJsonObject Root = Document.object();

		mFileId = Root["PublisherID"].toString().toULongLong();
		mTitle = Root["Title"].toString();
		mBriefingDescription = Root.value("Description").toString(Root.value("BriefingDescription").toString());
		mSteamDescription = Root.value("SteamDescription").toString(mBriefingDescription);
		mThumbnail = Root["Thumbnail"].toString();
		mTags = Root["Tags"].toString().split(',');
	}

	if (mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->RequestUGCDetails(mFileId, 10);
		mSteamCallResultRequestDetails.Set(SteamAPICall, this, &mlMainWindow::OnUGCRequestUGCDetails);
	}
	else
		ShowPublishDialog();
}

void mlMainWindow::OnUGCRequestUGCDetails(SteamUGCRequestUGCDetailsResult_t* RequestDetailsResult, bool IOFailure)
{
	if (IOFailure || RequestDetailsResult->m_details.m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", "Error retrieving item data from the Steam Workshop.");
		return;
	}

	SteamUGCDetails_t* Details = &RequestDetailsResult->m_details;

	if (mTitle.trimmed().isEmpty())
		mTitle = Details->m_rgchTitle;
	if (mBriefingDescription.trimmed().isEmpty())
		mBriefingDescription = Details->m_rgchDescription;
	if (mSteamDescription.trimmed().isEmpty())
		mSteamDescription = Details->m_rgchDescription;
	mTags = QString(Details->m_rgchTags).split(',');

	ShowPublishDialog();
}

void mlMainWindow::ShowPublishDialog()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Publish Mod");
	Dialog.resize(1120, 780);
	Dialog.setMinimumSize(1020, 720);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	Layout->setSpacing(14);
	const QString WorkshopJsonPath = QDir::cleanPath(QString("%1/workshop.json").arg(mWorkshopFolder));
	const QString WorkshopFolderPath = mWorkshopFolder;

	const QString ContentRoot = QFileInfo(mWorkshopFolder).dir().absolutePath();
	const QStringList BuiltLanguages = DetectBuiltLanguages(ContentRoot);
	if (HasOnlyEnglishBuild(ContentRoot))
	{
		QLabel* WarningLabel = new QLabel("Warning: this item currently appears to have only English localized files built. Uploading now may ship English-only Workshop content.");
		WarningLabel->setWordWrap(true);
		WarningLabel->setObjectName("WarningBanner");
		Layout->addWidget(WarningLabel);
	}
	else if (BuiltLanguages.isEmpty())
	{
		QLabel* WarningLabel = new QLabel("Warning: no localized build folders were detected for this item. Verify your latest link output before uploading.");
		WarningLabel->setWordWrap(true);
		WarningLabel->setObjectName("WarningBanner");
		Layout->addWidget(WarningLabel);
	}

	const bool AllLanguagesDetected = (BuiltLanguages.count() == ARRAYSIZE(gLanguages));
	QLabel* SummaryLabel = new QLabel(QString("Detected languages: %1").arg(BuiltLanguages.isEmpty() ? "none" : BuiltLanguages.join(", ")));
	SummaryLabel->setWordWrap(true);
	SummaryLabel->setObjectName(AllLanguagesDetected ? "SuccessBanner" : "WarningBanner");
	Layout->addWidget(SummaryLabel);

	QSplitter* Splitter = new QSplitter(&Dialog);
	Layout->addWidget(Splitter, 1);

	QWidget* EditorPanel = new QWidget(Splitter);
	QVBoxLayout* EditorLayout = new QVBoxLayout(EditorPanel);
	EditorLayout->setContentsMargins(0, 0, 0, 0);
	EditorLayout->setSpacing(12);

	QHBoxLayout* VersionLayout = new QHBoxLayout();
	VersionLayout->addWidget(new QLabel("Workshop Versions:"));
	QPushButton* VersionManagerButton = new QPushButton("Open Version Manager");
	VersionLayout->addWidget(VersionManagerButton);
	VersionLayout->addStretch(1);
	EditorLayout->addLayout(VersionLayout);

	QFormLayout* FormLayout = new QFormLayout();
	FormLayout->setSpacing(12);
	EditorLayout->addLayout(FormLayout);

	QLineEdit* TitleWidget = new QLineEdit();
	TitleWidget->setText(mTitle);
	QToolButton* TitleHelpButton = new QToolButton();
	TitleHelpButton->setText("?");
	TitleHelpButton->setToolTip("Title tips");
	QWidget* TitleRowWidget = new QWidget(&Dialog);
	QHBoxLayout* TitleRowLayout = new QHBoxLayout(TitleRowWidget);
	TitleRowLayout->setContentsMargins(0, 0, 0, 0);
	TitleRowLayout->setSpacing(6);
	TitleRowLayout->addWidget(TitleWidget, 1);
	TitleRowLayout->addWidget(TitleHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Title:", TitleRowWidget);

	QTextEdit* BriefingDescriptionWidget = new QTextEdit();
	BriefingDescriptionWidget->setAcceptRichText(false);
	BriefingDescriptionWidget->setPlainText(mBriefingDescription);
	BriefingDescriptionWidget->setMinimumHeight(120);
	BriefingDescriptionWidget->setPlaceholderText("Uploaded first. This is the in-game briefing description.");
	QToolButton* BriefingDescriptionHelpButton = new QToolButton();
	BriefingDescriptionHelpButton->setText("?");
	BriefingDescriptionHelpButton->setToolTip("Briefing description tips");
	QWidget* BriefingDescriptionRowWidget = new QWidget(&Dialog);
	QHBoxLayout* BriefingDescriptionRowLayout = new QHBoxLayout(BriefingDescriptionRowWidget);
	BriefingDescriptionRowLayout->setContentsMargins(0, 0, 0, 0);
	BriefingDescriptionRowLayout->setSpacing(6);
	BriefingDescriptionRowLayout->addWidget(BriefingDescriptionWidget, 1);
	BriefingDescriptionRowLayout->addWidget(BriefingDescriptionHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Briefing Description:", BriefingDescriptionRowWidget);

	QTextEdit* SteamDescriptionWidget = new QTextEdit();
	SteamDescriptionWidget->setAcceptRichText(false);
	SteamDescriptionWidget->setPlainText(mSteamDescription);
	SteamDescriptionWidget->setMinimumHeight(160);
	SteamDescriptionWidget->setPlaceholderText("Automatically pushed to the Workshop after upload. Leave blank to reuse the briefing description.");
	QToolButton* SteamDescriptionHelpButton = new QToolButton();
	SteamDescriptionHelpButton->setText("?");
	SteamDescriptionHelpButton->setToolTip("Steam description tips");
	QWidget* SteamDescriptionRowWidget = new QWidget(&Dialog);
	QHBoxLayout* SteamDescriptionRowLayout = new QHBoxLayout(SteamDescriptionRowWidget);
	SteamDescriptionRowLayout->setContentsMargins(0, 0, 0, 0);
	SteamDescriptionRowLayout->setSpacing(6);
	SteamDescriptionRowLayout->addWidget(SteamDescriptionWidget, 1);
	SteamDescriptionRowLayout->addWidget(SteamDescriptionHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Steam Description:", SteamDescriptionRowWidget);

	QLineEdit* ThumbnailEdit = new QLineEdit();
	ThumbnailEdit->setText(mThumbnail);

	QToolButton* ThumbnailButton = new QToolButton();
	ThumbnailButton->setText("...");
	QToolButton* ThumbnailHelpButton = new QToolButton();
	ThumbnailHelpButton->setText("?");
	ThumbnailHelpButton->setToolTip(
		"__**High Quality Steam Workshop Thumbnails:**__\n"
		"**File Size:** Under 1MB ( Anything over will result in launcher giving the \"steam error code\" message )\n"
		"**File Resolution:** 2048x2048 ( You can use any power of 2 as long as the file size is under 1MB )\n"
		"**File Format:** PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )\n\n"
		"__**High Quality Steam Workshop Preview Images:**__\n"
		"**File Size:** Under 2MB\n"
		"**File Resolution:** 1920 x 1080 ( Any resolution is fine as long as you are under 2MB in file size - some resolutions will be scaled by steam )\n"
		"**File Format:** PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )\n\n"
		"*Gifs are also an option, but make sure to stay under the 1 MB*");

	QHBoxLayout* ThumbnailLayout = new QHBoxLayout();
	ThumbnailLayout->setContentsMargins(0, 0, 0, 0);
	ThumbnailLayout->addWidget(ThumbnailEdit);
	ThumbnailLayout->addWidget(ThumbnailButton);
	ThumbnailLayout->addWidget(ThumbnailHelpButton);

	QWidget* ThumbnailWidget = new QWidget();
	ThumbnailWidget->setLayout(ThumbnailLayout);

	FormLayout->addRow("Thumbnail:", ThumbnailWidget);

	QWidget* TagsWidget = new QWidget(&Dialog);
	QGridLayout* TagsLayout = new QGridLayout(TagsWidget);
	TagsLayout->setContentsMargins(0, 0, 0, 0);
	TagsLayout->setHorizontalSpacing(12);
	TagsLayout->setVerticalSpacing(6);
	QList<QCheckBox*> TagCheckboxes;
	for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
	{
		QCheckBox* TagBox = new QCheckBox(gTags[TagIdx], TagsWidget);
		TagBox->setChecked(mTags.contains(gTags[TagIdx]));
		TagsLayout->addWidget(TagBox, TagIdx / 4, TagIdx % 4);
		TagCheckboxes.append(TagBox);
	}
	QWidget* TagsRowWidget = new QWidget(&Dialog);
	QHBoxLayout* TagsRowLayout = new QHBoxLayout(TagsRowWidget);
	TagsRowLayout->setContentsMargins(0, 0, 0, 0);
	TagsRowLayout->setSpacing(6);
	TagsRowLayout->addWidget(TagsWidget, 1);
	QToolButton* TagsHelpButton = new QToolButton(TagsRowWidget);
	TagsHelpButton->setText("?");
	TagsHelpButton->setToolTip("Tag tips");
	TagsRowLayout->addWidget(TagsHelpButton, 0, Qt::AlignTop);
	FormLayout->addRow("Tags:", TagsRowWidget);

	QWidget* PreviewPanel = new QWidget(Splitter);
	QVBoxLayout* PreviewLayout = new QVBoxLayout(PreviewPanel);
	PreviewLayout->setContentsMargins(10, 0, 0, 0);
	PreviewLayout->setSpacing(10);
	QWidget* SummaryPanel = new QWidget(PreviewPanel);
	QHBoxLayout* SummaryLayout = new QHBoxLayout(SummaryPanel);
	SummaryLayout->setContentsMargins(0, 0, 0, 0);
	SummaryLayout->setSpacing(12);
	QLabel* ThumbnailPreview = new QLabel("No Preview");
	ThumbnailPreview->setObjectName("BackgroundPreview");
	ThumbnailPreview->setFixedSize(118, 118);
	ThumbnailPreview->setAlignment(Qt::AlignCenter);
	auto ResolveThumbnailPath = [=](const QString& ThumbnailValue) -> QString
	{
		if (ThumbnailValue.trimmed().isEmpty())
			return QString();
		QFileInfo ThumbInfo(ThumbnailValue);
		if (ThumbInfo.isAbsolute())
			return ThumbInfo.absoluteFilePath();
		return QDir::cleanPath(QString("%1/%2").arg(WorkshopFolderPath, ThumbnailValue));
	};
	UpdateBackgroundPreviewLabel(ThumbnailPreview, ResolveThumbnailPath(mThumbnail));
	SummaryLayout->addWidget(ThumbnailPreview, 0, Qt::AlignTop);
	QVBoxLayout* SummaryTextLayout = new QVBoxLayout();
	SummaryTextLayout->setContentsMargins(0, 0, 0, 0);
	SummaryTextLayout->setSpacing(4);
	QLabel* PreviewTitle = new QLabel("Steam Workshop Preview");
	SummaryTextLayout->addWidget(PreviewTitle);
	QLabel* PreviewTitleValue = new QLabel(StripTreyarchColorCodes(mTitle));
	PreviewTitleValue->setWordWrap(true);
	PreviewTitleValue->setStyleSheet("font-weight:700;");
	SummaryTextLayout->addWidget(PreviewTitleValue);
	QLabel* PreviewIdValue = new QLabel(QString("Workshop ID: %1").arg(mFileId ? QString::number(mFileId) : "new item"));
	PreviewIdValue->setWordWrap(true);
	SummaryTextLayout->addWidget(PreviewIdValue);
	QLabel* PreviewTagsValue = new QLabel(QString("Tags: %1").arg(mTags.isEmpty() ? "none" : mTags.join(", ")));
	PreviewTagsValue->setWordWrap(true);
	SummaryTextLayout->addWidget(PreviewTagsValue);
	SummaryTextLayout->addStretch(1);
	SummaryLayout->addLayout(SummaryTextLayout, 1);
	PreviewLayout->addWidget(SummaryPanel);
	QLabel* DescriptionPreviewTitle = new QLabel("Steam Description Preview");
	PreviewLayout->addWidget(DescriptionPreviewTitle);
	QTextBrowser* PreviewWidget = new QTextBrowser(PreviewPanel);
	PreviewWidget->setObjectName("MarkdownPreview");
	PreviewWidget->setOpenExternalLinks(true);
	PreviewWidget->document()->setDocumentMargin(8);
	PreviewWidget->setHtml(SteamMarkupToHtml(SteamDescriptionForUpload(mBriefingDescription, mSteamDescription)));
	PreviewWidget->setMinimumWidth(540);
	PreviewLayout->addWidget(PreviewWidget, 1);
	auto RefreshPreviewMeta = [=, &TagCheckboxes]()
	{
		QStringList ActiveTags;
		for (QCheckBox* TagBox : TagCheckboxes)
		{
			if (TagBox->isChecked())
				ActiveTags.append(TagBox->text());
		}
		PreviewTitleValue->setText(StripTreyarchColorCodes(TitleWidget->text()));
		PreviewIdValue->setText(QString("Workshop ID: %1").arg(mFileId ? QString::number(mFileId) : "new item"));
		PreviewTagsValue->setText(QString("Tags: %1").arg(ActiveTags.isEmpty() ? "none" : ActiveTags.join(", ")));
		PreviewWidget->setHtml(SteamMarkupToHtml(SteamDescriptionForUpload(BriefingDescriptionWidget->toPlainText(), SteamDescriptionWidget->toPlainText())));
	};
	connect(BriefingDescriptionWidget, &QTextEdit::textChanged, &Dialog, RefreshPreviewMeta);
	connect(SteamDescriptionWidget, &QTextEdit::textChanged, &Dialog, RefreshPreviewMeta);
	connect(TitleWidget, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshPreviewMeta(); });
	for (QCheckBox* TagBox : TagCheckboxes)
		connect(TagBox, &QCheckBox::toggled, &Dialog, [=](bool) { RefreshPreviewMeta(); });
	Splitter->setHandleWidth(14);
	Splitter->setStretchFactor(0, 3);
	Splitter->setStretchFactor(1, 4);
	Splitter->setSizes(QList<int>() << 430 << 690);

	QFrame* Frame = new QFrame();
	Frame->setFrameShape(QFrame::HLine);
	Frame->setFrameShadow(QFrame::Raised);
	Layout->addWidget(Frame);

	QHBoxLayout* BottomButtonsLayout = new QHBoxLayout();
	BottomButtonsLayout->addStretch(1);
	QPushButton* SaveInfoButton = new QPushButton("Save Info", &Dialog);
	QPushButton* CancelButton = new QPushButton("Cancel", &Dialog);
	QPushButton* UploadButton = new QPushButton("Upload", &Dialog);
	UploadButton->setDefault(true);
	UploadButton->setAutoDefault(true);
	UploadButton->setStyleSheet("background:#2f8f47; border:1px solid #4fcb72; color:#ffffff; font-weight:700; padding:8px 18px; border-radius:8px;");
	BottomButtonsLayout->addWidget(SaveInfoButton);
	BottomButtonsLayout->addWidget(CancelButton);
	BottomButtonsLayout->addWidget(UploadButton);
	Layout->addLayout(BottomButtonsLayout);

	auto ThumbnailBrowse = [this, ThumbnailEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(this, "Open Thumbnail", QString(), "All Files (*.*)");
		if (!FileName.isEmpty())
			ThumbnailEdit->setText(FileName);
	};
	auto ShowThumbnailGuide = [this]()
	{
		QDialog GuideDialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
		GuideDialog.setWindowTitle("Thumbnail Guide");
		GuideDialog.resize(640, 420);
		QVBoxLayout* GuideLayout = new QVBoxLayout(&GuideDialog);
		QTextBrowser* GuideText = new QTextBrowser(&GuideDialog);
		GuideText->setOpenExternalLinks(true);
		GuideText->setHtml(
			"<h3>High Quality Steam Workshop Thumbnails</h3>"
			"<p><b>File Size:</b> Under 1MB ( Anything over will result in launcher giving the \"steam error code\" message )</p>"
			"<p><b>File Resolution:</b> 2048x2048 ( You can use any power of 2 as long as the file size is under 1MB )</p>"
			"<p><b>File Format:</b> PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )</p>"
			"<h3>High Quality Steam Workshop Preview Images</h3>"
			"<p><b>File Size:</b> Under 2MB</p>"
			"<p><b>File Resolution:</b> 1920 x 1080 ( Any resolution is fine as long as you are under 2MB in file size - some resolutions will be scaled by steam )</p>"
			"<p><b>File Format:</b> PNG or JPG ( JPG will have smaller file sizes, but comes with a slight loss in quality )</p>"
			"<p><i>Gifs are also an option, but make sure to stay under the 1 MB</i></p>");
		GuideLayout->addWidget(GuideText, 1);
		QDialogButtonBox* GuideButtons = new QDialogButtonBox(QDialogButtonBox::Close, &GuideDialog);
		connect(GuideButtons, SIGNAL(rejected()), &GuideDialog, SLOT(reject()));
		GuideLayout->addWidget(GuideButtons);
		GuideDialog.exec();
	};
	auto ShowBriefingDescriptionGuide = [this]()
	{
		QMessageBox::information(this, "Briefing Description Guide", "Briefing Description is uploaded first. Use it for the short in-game description shown to players when the content is installed.");
	};
	auto ShowSteamDescriptionGuide = [this]()
	{
		QMessageBox::information(this, "Steam Description Guide", "Steam Description is pushed automatically right after upload finishes. Leave it blank if you want the Workshop page to reuse the Briefing Description.");
	};
	auto ShowTitleGuide = [this]()
	{
		QMessageBox::information(this, "Title Guide",
			"You can use Treyarch color codes in the title.\n\n"
			"^1 Red\n"
			"^2 Green\n"
			"^3 Yellow\n"
			"^4 Blue\n"
			"^5 Cyan\n"
			"^6 Fuchsia\n"
			"^7 White\n"
			"^8 Light Blue\n"
			"^9 Orange\n"
			"^0 Black\n\n"
			"After upload, the launcher automatically removes these color codes from the Workshop title while keeping the colored title for the briefing upload step.");
	};
	auto RefreshPublishFieldsFromWorkshopJson = [this, WorkshopJsonPath, TitleWidget, BriefingDescriptionWidget, SteamDescriptionWidget, ThumbnailEdit, &TagCheckboxes]()
	{
		QFile WorkshopFile(WorkshopJsonPath);
		if (!WorkshopFile.open(QIODevice::ReadOnly))
			return;
		const QJsonObject Root = QJsonDocument::fromJson(WorkshopFile.readAll()).object();
		mFileId = Root.value("PublisherID").toString().toULongLong();
		TitleWidget->setText(Root.value("Title").toString());
		BriefingDescriptionWidget->setPlainText(Root.value("Description").toString(Root.value("BriefingDescription").toString()));
		SteamDescriptionWidget->setPlainText(Root.value("SteamDescription").toString(BriefingDescriptionWidget->toPlainText()));
		ThumbnailEdit->setText(Root.value("Thumbnail").toString());
		const QStringList VersionTags = Root.value("Tags").toString().split(',', Qt::SkipEmptyParts);
		for (QCheckBox* TagBox : TagCheckboxes)
			TagBox->setChecked(VersionTags.contains(TagBox->text()));
	};

	connect(ThumbnailButton, &QToolButton::clicked, ThumbnailBrowse);
	connect(ThumbnailHelpButton, &QToolButton::clicked, &Dialog, ShowThumbnailGuide);
	connect(TitleHelpButton, &QToolButton::clicked, &Dialog, ShowTitleGuide);
	connect(BriefingDescriptionHelpButton, &QToolButton::clicked, &Dialog, ShowBriefingDescriptionGuide);
	connect(SteamDescriptionHelpButton, &QToolButton::clicked, &Dialog, ShowSteamDescriptionGuide);
	connect(TagsHelpButton, &QToolButton::clicked, &Dialog, [this]()
	{
		QMessageBox::information(this, "Tag Guide", "Choose the tags that best match your map or mod so it is easier to find on the Workshop. You can update tags later too.");
	});
	connect(ThumbnailEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(ThumbnailPreview, ResolveThumbnailPath(Text));
		RefreshPreviewMeta();
	});
	connect(VersionManagerButton, &QPushButton::clicked, &Dialog, [=, &TagCheckboxes]()
	{
		QTreeWidgetItem TempItem(QStringList() << mFolderName);
		TempItem.setData(0, Qt::UserRole, mType == "map" ? ML_ITEM_MAP : ML_ITEM_MOD_GROUP);
		TempItem.setData(0, ML_ITEM_CONTAINER_ROLE, mFolderName);
		TempItem.setData(0, ML_ITEM_NAME_ROLE, mFolderName);
		ShowWorkshopVersionsDialog(&TempItem);
		RefreshPublishFieldsFromWorkshopJson();
		RefreshPreviewMeta();
	});
	connect(CancelButton, &QPushButton::clicked, &Dialog, &QDialog::reject);
	connect(UploadButton, &QPushButton::clicked, &Dialog, &QDialog::accept);
	connect(SaveInfoButton, &QPushButton::clicked, &Dialog, [=, &Dialog, &TagCheckboxes]()
	{
		mTitle = TitleWidget->text();
		mBriefingDescription = BriefingDescriptionWidget->toPlainText();
		mSteamDescription = SteamDescriptionWidget->toPlainText();
		mThumbnail = ThumbnailEdit->text();
		mTags.clear();
		for (QCheckBox* TagBox : TagCheckboxes)
		{
			if (TagBox->isChecked())
				mTags.append(TagBox->text());
		}

		if (SaveWorkshopMetadata())
			QMessageBox::information(&Dialog, "Workshop Info", QString("Saved %1 without uploading.").arg(QDir::toNativeSeparators(WorkshopJsonPath)));
	});

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mTitle = TitleWidget->text();
	mBriefingDescription = BriefingDescriptionWidget->toPlainText();
	mSteamDescription = SteamDescriptionWidget->toPlainText();
	mThumbnail = ThumbnailEdit->text();
	mTags.clear();
	mPostUploadSteamSyncPending = (StripTreyarchColorCodes(mTitle) != mTitle.trimmed()) || (SteamDescriptionForUpload(mBriefingDescription, mSteamDescription) != mBriefingDescription);

	for (QCheckBox* TagBox : TagCheckboxes)
	{
		if (TagBox->isChecked())
			mTags.append(TagBox->text());
	}

	if (!SteamUGC())
	{
		QMessageBox::information(this, "Error", "Could not initialize Steam, make sure you're running the launcher from the Steam client.");
		return;
	}

	if (!mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->CreateItem(AppId, k_EWorkshopFileTypeCommunity);
		mSteamCallResultCreateItem.Set(SteamAPICall, this, &mlMainWindow::OnCreateItemResult);
	}
	else
		UpdateWorkshopItem(true);
}

void mlMainWindow::OnEditOptions()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Options");
	Dialog.resize(980, 760);
	Dialog.setMinimumWidth(920);

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);
	QSettings Settings;
	QHBoxLayout* SettingsContentLayout = new QHBoxLayout();
	QListWidget* SettingsNavList = new QListWidget(&Dialog);
	SettingsNavList->setObjectName("SettingsNavList");
	SettingsNavList->setSpacing(2);
	SettingsNavList->setIconSize(QSize(18, 18));
	SettingsNavList->setFixedWidth(188);
	SettingsNavList->setSelectionMode(QAbstractItemView::SingleSelection);
	QStackedWidget* SettingsPages = new QStackedWidget(&Dialog);
	SettingsContentLayout->addWidget(SettingsNavList);
	SettingsContentLayout->addWidget(SettingsPages, 1);
	Layout->addLayout(SettingsContentLayout, 1);
	auto AddSettingsPage = [&](const QString& Label, const QIcon& Icon, QWidget* Page)
	{
		SettingsPages->addWidget(Page);
		QListWidgetItem* Item = new QListWidgetItem(Icon, Label, SettingsNavList);
		Item->setSizeHint(QSize(170, 38));
	};

	QWidget* GeneralTab = new QWidget(&Dialog);
	QVBoxLayout* GeneralLayout = new QVBoxLayout(GeneralTab);
	GeneralLayout->setContentsMargins(14, 14, 14, 14);
	GeneralLayout->setSpacing(12);

	GeneralLayout->addWidget(new QLabel("Pinned Sections"));
	QCheckBox* ShowRecentsCheckbox = new QCheckBox("Enable Recents");
	ShowRecentsCheckbox->setChecked(Settings.value("ShowRecents", true).toBool());
	GeneralLayout->addWidget(ShowRecentsCheckbox);
	QCheckBox* ShowFavoritesCheckbox = new QCheckBox("Enable Favorites");
	ShowFavoritesCheckbox->setChecked(Settings.value("ShowFavorites", true).toBool());
	GeneralLayout->addWidget(ShowFavoritesCheckbox);

	QFrame* PinnedFrame = new QFrame();
	PinnedFrame->setFrameShape(QFrame::HLine);
	GeneralLayout->addWidget(PinnedFrame);

	QHBoxLayout* LanguageLayout = new QHBoxLayout();
	LanguageLayout->addWidget(new QLabel("Build Language:"));

	QStringList Languages;
	Languages << "All";
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
		Languages << gLanguages[LanguageIdx];

	QComboBox* LanguageCombo = new QComboBox();
	LanguageCombo->addItems(Languages);
	LanguageCombo->setCurrentText(mBuildLanguage);
	LanguageLayout->addWidget(LanguageCombo);
	GeneralLayout->addLayout(LanguageLayout);
	QFrame* ApeFrame = new QFrame();
	ApeFrame->setFrameShape(QFrame::HLine);
	GeneralLayout->addWidget(ApeFrame);
	GeneralLayout->addWidget(new QLabel("APE"));
	QLabel* ApeCacheLabel = new QLabel("Delete the Asset Property Editor cache file if APE needs a clean refresh.");
	ApeCacheLabel->setWordWrap(true);
	GeneralLayout->addWidget(ApeCacheLabel);
	QPushButton* DeleteApeCacheButton = new QPushButton("Delete Cache");
	GeneralLayout->addWidget(DeleteApeCacheButton, 0, Qt::AlignLeft);
	GeneralLayout->addStretch(1);
	AddSettingsPage("General", style()->standardIcon(QStyle::SP_FileDialogDetailedView), GeneralTab);

	QWidget* ThemesTab = new QWidget(&Dialog);
	QVBoxLayout* ThemesLayout = new QVBoxLayout(ThemesTab);
	ThemesLayout->setContentsMargins(14, 14, 14, 14);
	ThemesLayout->setSpacing(12);

	ThemesLayout->addWidget(new QLabel("Theme"));
	QHBoxLayout* ThemePickerLayout = new QHBoxLayout();
	QComboBox* ThemeProfileCombo = new QComboBox(&Dialog);
	for (const QString& ThemeProfileId : AvailableThemeProfileIds())
		ThemeProfileCombo->addItem(ThemeProfileDisplayName(ThemeProfileId), ThemeProfileId);
	const int CurrentThemeIndex = ThemeProfileCombo->findData(CurrentThemeProfileId());
	ThemeProfileCombo->setCurrentIndex(CurrentThemeIndex >= 0 ? CurrentThemeIndex : 0);
	ThemePickerLayout->addWidget(ThemeProfileCombo, 1);
	QPushButton* SaveThemeButton = new QPushButton("Save Theme");
	SaveThemeButton->setToolTip("Save the current theme settings into the selected theme profile.");
	ThemePickerLayout->addWidget(SaveThemeButton);
	QPushButton* SaveThemeAsButton = new QPushButton("Save As New");
	SaveThemeAsButton->setToolTip("Create a new saved theme from the current settings.");
	ThemePickerLayout->addWidget(SaveThemeAsButton);
	QPushButton* DeleteThemeButton = new QPushButton("Delete");
	DeleteThemeButton->setToolTip("Delete the selected custom theme.");
	ThemePickerLayout->addWidget(DeleteThemeButton);
	ThemesLayout->addLayout(ThemePickerLayout);

	QHBoxLayout* BaseThemeLayout = new QHBoxLayout();
	BaseThemeLayout->addWidget(new QLabel("Base Style:"));
	QComboBox* BaseThemeCombo = new QComboBox(&Dialog);
	BaseThemeCombo->addItem("Original Updated", ThemeOriginalUpdated);
	BaseThemeCombo->addItem("Original Classic", ThemeOriginalClassic);
	BaseThemeCombo->addItem("Dark Modern", ThemeDarkModern);
	BaseThemeCombo->setCurrentIndex(qMax(0, BaseThemeCombo->findData(mThemeMode)));
	BaseThemeLayout->addWidget(BaseThemeCombo, 1);
	ThemesLayout->addLayout(BaseThemeLayout);

	QFrame* ThemeFrame = new QFrame();
	ThemeFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(ThemeFrame);

	ThemesLayout->addWidget(new QLabel("Appearance"));

	QHBoxLayout* AccentLayout = new QHBoxLayout();
	AccentLayout->addWidget(new QLabel("Accent Color:"));
	QLineEdit* AccentColorEdit = new QLineEdit(Settings.value("AccentColor", "#ff8a2a").toString());
	AccentLayout->addWidget(AccentColorEdit);
	QPushButton* AccentBrowseButton = new QPushButton();
	AccentBrowseButton->setObjectName("AccentSwatchButton");
	AccentBrowseButton->setFixedSize(26, 26);
	AccentBrowseButton->setToolTip("Pick accent color");
	AccentLayout->addWidget(AccentBrowseButton);
	ThemesLayout->addLayout(AccentLayout);

	QCheckBox* ShowItemTypeTags = new QCheckBox("Show Map / Mod tags");
	ShowItemTypeTags->setChecked(Settings.value("ShowItemTypeTags", true).toBool());
	ShowItemTypeTags->setToolTip("Show the small Map/Mod label next to each main item name.");
	ThemesLayout->addWidget(ShowItemTypeTags);

	QHBoxLayout* AssetBackgroundLayout = new QHBoxLayout();
	AssetBackgroundLayout->addWidget(new QLabel("List Background:"));
	BackgroundDropLineEdit* AssetBackgroundEdit = new BackgroundDropLineEdit(Settings.value("AssetTreeBackgroundImage", "").toString());
	AssetBackgroundLayout->addWidget(AssetBackgroundEdit);
	QSlider* AssetBackgroundOpacitySlider = new QSlider(Qt::Horizontal);
	AssetBackgroundOpacitySlider->setRange(0, 100);
	AssetBackgroundOpacitySlider->setValue(Settings.value("AssetTreeBackgroundOpacity", 100).toInt());
	AssetBackgroundOpacitySlider->setMinimumWidth(110);
	AssetBackgroundLayout->addWidget(AssetBackgroundOpacitySlider);
	QSpinBox* AssetBackgroundOpacity = new QSpinBox();
	AssetBackgroundOpacity->setRange(0, 100);
	AssetBackgroundOpacity->setSuffix("%");
	AssetBackgroundOpacity->setValue(Settings.value("AssetTreeBackgroundOpacity", 100).toInt());
	AssetBackgroundLayout->addWidget(AssetBackgroundOpacity);
	QLabel* AssetBackgroundPreview = new QLabel("None");
	AssetBackgroundPreview->setObjectName("BackgroundPreview");
	AssetBackgroundPreview->setFixedSize(52, 52);
	AssetBackgroundPreview->setAlignment(Qt::AlignCenter);
	AssetBackgroundPreview->setToolTip("Current list background preview");
	AssetBackgroundLayout->addWidget(AssetBackgroundPreview);
	QPushButton* AssetBackgroundBrowse = new QPushButton("Browse...");
	AssetBackgroundLayout->addWidget(AssetBackgroundBrowse);
	QPushButton* AssetBackgroundRemove = new QPushButton("X");
	AssetBackgroundRemove->setToolTip("Remove list background");
	AssetBackgroundRemove->setFixedWidth(28);
	AssetBackgroundLayout->addWidget(AssetBackgroundRemove);
	ThemesLayout->addLayout(AssetBackgroundLayout);

	QHBoxLayout* LogBackgroundLayout = new QHBoxLayout();
	LogBackgroundLayout->addWidget(new QLabel("Log Background:"));
	BackgroundDropLineEdit* LogBackgroundEdit = new BackgroundDropLineEdit(Settings.value("LogBackgroundImage", "").toString());
	LogBackgroundLayout->addWidget(LogBackgroundEdit);
	QSlider* LogBackgroundOpacitySlider = new QSlider(Qt::Horizontal);
	LogBackgroundOpacitySlider->setRange(0, 100);
	LogBackgroundOpacitySlider->setValue(Settings.value("LogBackgroundOpacity", 100).toInt());
	LogBackgroundOpacitySlider->setMinimumWidth(110);
	LogBackgroundLayout->addWidget(LogBackgroundOpacitySlider);
	QSpinBox* LogBackgroundOpacity = new QSpinBox();
	LogBackgroundOpacity->setRange(0, 100);
	LogBackgroundOpacity->setSuffix("%");
	LogBackgroundOpacity->setValue(Settings.value("LogBackgroundOpacity", 100).toInt());
	LogBackgroundLayout->addWidget(LogBackgroundOpacity);
	QLabel* LogBackgroundPreview = new QLabel("None");
	LogBackgroundPreview->setObjectName("BackgroundPreview");
	LogBackgroundPreview->setFixedSize(52, 52);
	LogBackgroundPreview->setAlignment(Qt::AlignCenter);
	LogBackgroundPreview->setToolTip("Current log background preview");
	LogBackgroundLayout->addWidget(LogBackgroundPreview);
	QPushButton* LogBackgroundBrowse = new QPushButton("Browse...");
	LogBackgroundLayout->addWidget(LogBackgroundBrowse);
	QPushButton* LogBackgroundRemove = new QPushButton("X");
	LogBackgroundRemove->setToolTip("Remove log background");
	LogBackgroundRemove->setFixedWidth(28);
	LogBackgroundLayout->addWidget(LogBackgroundRemove);
	ThemesLayout->addLayout(LogBackgroundLayout);

	QFrame* LayoutFrame = new QFrame();
	LayoutFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(LayoutFrame);

	ThemesLayout->addWidget(new QLabel("Layout"));
	auto CreateLayoutPreviewIcon = [&](const QString& LayoutKey) -> QIcon
	{
		QPixmap Pixmap(64, 40);
		Pixmap.fill(Qt::transparent);
		QPainter Painter(&Pixmap);
		Painter.setRenderHint(QPainter::Antialiasing, true);
		Painter.setPen(QPen(QColor(90, 98, 108), 1));
		Painter.setBrush(QColor(24, 28, 33));
		Painter.drawRoundedRect(QRectF(1, 1, 62, 38), 6, 6);

		auto DrawPanel = [&](const QRect& Rect, const QColor& Color)
		{
			Painter.setPen(Qt::NoPen);
			Painter.setBrush(Color);
			Painter.drawRoundedRect(Rect, 3, 3);
		};

		if (LayoutKey == "left-build-console")
		{
			DrawPanel(QRect(4, 4, 20, 32), QColor(42, 54, 68));
			DrawPanel(QRect(27, 4, 12, 32), QColor(80, 62, 44));
			DrawPanel(QRect(42, 4, 18, 32), QColor(22, 24, 27));
		}
		else if (LayoutKey == "left-console-build")
		{
			DrawPanel(QRect(4, 4, 20, 32), QColor(42, 54, 68));
			DrawPanel(QRect(27, 4, 18, 32), QColor(22, 24, 27));
			DrawPanel(QRect(48, 4, 12, 32), QColor(80, 62, 44));
		}
		else if (LayoutKey == "original")
		{
			DrawPanel(QRect(4, 4, 38, 16), QColor(40, 52, 66));
			DrawPanel(QRect(45, 4, 15, 16), QColor(80, 62, 44));
			DrawPanel(QRect(4, 23, 56, 13), QColor(22, 24, 27));
		}
		else
		{
			DrawPanel(QRect(4, 4, 38, 14), QColor(40, 52, 66));
			DrawPanel(QRect(45, 4, 15, 14), QColor(80, 62, 44));
			DrawPanel(QRect(4, 21, 56, 15), QColor(22, 24, 27));
		}

		return QIcon(Pixmap);
	};

	QButtonGroup* LayoutGroup = new QButtonGroup(&Dialog);
	QHBoxLayout* LayoutOptionsLayout = new QHBoxLayout();
	struct LayoutOption
	{
		const char* Key;
		const char* Label;
	};
	const LayoutOption LayoutOptions[] =
	{
		{ "original", "Original" },
		{ "left-build-console", "Maps / Build / Console" },
		{ "left-console-build", "Maps / Console / Build" }
	};
	QHash<QString, QAbstractButton*> LayoutButtons;
	for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
	{
		const LayoutOption Option = LayoutOptions[LayoutIdx];
		QToolButton* LayoutButton = new QToolButton(&Dialog);
		LayoutButton->setCheckable(true);
		LayoutButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		LayoutButton->setIcon(CreateLayoutPreviewIcon(Option.Key));
		LayoutButton->setIconSize(QSize(64, 40));
		LayoutButton->setText(Option.Label);
		LayoutButton->setAutoRaise(false);
		LayoutButton->setMinimumWidth(120);
		LayoutGroup->addButton(LayoutButton);
		LayoutButtons.insert(Option.Key, LayoutButton);
		LayoutOptionsLayout->addWidget(LayoutButton);
	}
	QString SavedLayoutKey = mLauncherLayout.trimmed().isEmpty() ? QString("original") : mLauncherLayout.trimmed().toLower();
	if (SavedLayoutKey == "current")
		SavedLayoutKey = "original";
	if (LayoutButtons.contains(SavedLayoutKey))
		LayoutButtons.value(SavedLayoutKey)->setChecked(true);
	else if (LayoutButtons.contains("original"))
		LayoutButtons.value("original")->setChecked(true);
	ThemesLayout->addLayout(LayoutOptionsLayout);

	QFrame* CustomCssFrame = new QFrame();
	CustomCssFrame->setFrameShape(QFrame::HLine);
	ThemesLayout->addWidget(CustomCssFrame);

	ThemesLayout->addWidget(new QLabel("Custom CSS"));
	QPlainTextEdit* CustomCssEdit = new QPlainTextEdit(Settings.value("CustomStylesheet", "").toString());
	CustomCssEdit->setPlaceholderText("Add extra Qt stylesheet rules here...");
	CustomCssEdit->setMinimumHeight(120);
	ThemesLayout->addWidget(CustomCssEdit);
	QLabel* CustomCssHint = new QLabel("Tip: visible asset rows use custom widgets, so row styling usually needs #ItemRowWidget, #ItemTitleWidget, or #QuickActionStrip rather than only #AssetTree::item.");
	CustomCssHint->setWordWrap(true);
	ThemesLayout->addWidget(CustomCssHint);
	QPushButton* ApplyCustomCssButton = new QPushButton("Apply CSS");
	ApplyCustomCssButton->setToolTip("Apply the custom stylesheet immediately without closing options.");
	ThemesLayout->addWidget(ApplyCustomCssButton, 0, Qt::AlignLeft);

	ThemesLayout->addStretch(1);
	AddSettingsPage("Themes", style()->standardIcon(QStyle::SP_DesktopIcon), ThemesTab);

	QWidget* ConsoleTab = new QWidget(&Dialog);
	QVBoxLayout* ConsoleLayout = new QVBoxLayout(ConsoleTab);
	ConsoleLayout->setContentsMargins(14, 14, 14, 14);
	ConsoleLayout->setSpacing(12);

	ConsoleLayout->addWidget(new QLabel("Console Style"));
	QRadioButton* OriginalConsoleStyle = new QRadioButton("Original");
	QRadioButton* ImprovedConsoleStyle = new QRadioButton("Improved");
	if (UseImprovedConsoleStyle(Settings))
		ImprovedConsoleStyle->setChecked(true);
	else
		OriginalConsoleStyle->setChecked(true);
	ConsoleLayout->addWidget(OriginalConsoleStyle);
	ConsoleLayout->addWidget(ImprovedConsoleStyle);

	QFrame* LogColorsFrame = new QFrame();
	LogColorsFrame->setFrameShape(QFrame::HLine);
	ConsoleLayout->addWidget(LogColorsFrame);

	ConsoleLayout->addWidget(new QLabel("Log Colors"));
	struct LogColorField
	{
		QString Label;
		QString Key;
		QString DefaultValue;
	};
	const LogColorField LogColorFields[] = {
		{ "Default", "LogColors/Default", "#d7dce2" },
		{ "Command", "LogColors/Command", "#7dcfff" },
		{ "Info", "LogColors/Info", "#eef1f4" },
		{ "Launch", "LogColors/Launch", "#c792ea" },
		{ "Success", "LogColors/Success", "#6ee7a8" },
		{ "Warning", "LogColors/Warning", "#ffcf70" },
		{ "Error", "LogColors/Error", "#ff7a7a" }
	};
	QHash<QString, QLineEdit*> LogColorEdits;
	for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
	{
		const LogColorField& Field = LogColorFields[FieldIdx];
		QHBoxLayout* ColorLayout = new QHBoxLayout();
		ColorLayout->addWidget(new QLabel(Field.Label + ":"));
		QLineEdit* ColorEdit = new QLineEdit(Settings.value(Field.Key, Field.DefaultValue).toString());
		ColorEdit->setPlaceholderText(Field.DefaultValue);
		ColorLayout->addWidget(ColorEdit, 1);
		QPushButton* BrowseButton = new QPushButton();
		BrowseButton->setFixedSize(26, 26);
		BrowseButton->setToolTip(QString("Pick %1 log color").arg(Field.Label.toLower()));
		ColorLayout->addWidget(BrowseButton);
		ConsoleLayout->addLayout(ColorLayout);
		LogColorEdits.insert(Field.Key, ColorEdit);

		auto RefreshButton = [=]()
		{
			const QString SafeColor = NormalizedStoredColor(ColorEdit->text()).isEmpty() ? Field.DefaultValue : NormalizedStoredColor(ColorEdit->text());
			BrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
		};
		connect(BrowseButton, &QPushButton::clicked, &Dialog, [&, ColorEdit, Field]()
		{
			QColor Color = QColorDialog::getColor(QColor(ColorEdit->text()), &Dialog, QString("Select %1 Log Color").arg(Field.Label));
			if (Color.isValid())
				ColorEdit->setText(Color.name(QColor::HexRgb));
		});
		connect(ColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString&) { RefreshButton(); });
		RefreshButton();
	}

	ConsoleLayout->addStretch(1);
	AddSettingsPage("Console", style()->standardIcon(QStyle::SP_MessageBoxInformation), ConsoleTab);
	connect(SettingsNavList, &QListWidget::currentRowChanged, SettingsPages, &QStackedWidget::setCurrentIndex);
	SettingsNavList->setCurrentRow(0);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));
	connect(AccentBrowseButton, &QPushButton::clicked, &Dialog, [&, AccentColorEdit]()
	{
		QColor Color = QColorDialog::getColor(QColor(AccentColorEdit->text()), &Dialog, "Select Accent Color");
		if (Color.isValid())
			AccentColorEdit->setText(Color.name(QColor::HexRgb));
	});
	connect(AccentColorEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		const QColor PreviewColor(Text.trimmed());
		const QString SafeColor = PreviewColor.isValid() ? PreviewColor.name(QColor::HexRgb) : QString("#444444");
		AccentBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	});
	AccentColorEdit->setToolTip("Controls orange-highlighted UI elements like active tabs, buttons, and hover accents.");
	AccentColorEdit->setPlaceholderText("#ff8a2a");
	AccentColorEdit->setText(AccentColorEdit->text());
	connect(AssetBackgroundBrowse, &QPushButton::clicked, &Dialog, [&, AssetBackgroundEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select List Background", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!FileName.isEmpty())
			AssetBackgroundEdit->setText(FileName);
	});
	connect(LogBackgroundBrowse, &QPushButton::clicked, &Dialog, [&, LogBackgroundEdit]()
	{
		QString FileName = QFileDialog::getOpenFileName(&Dialog, "Select Log Background", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!FileName.isEmpty())
			LogBackgroundEdit->setText(FileName);
	});
	connect(AssetBackgroundRemove, &QPushButton::clicked, &Dialog, [=]()
	{
		AssetBackgroundEdit->clear();
	});
	connect(LogBackgroundRemove, &QPushButton::clicked, &Dialog, [=]()
	{
		LogBackgroundEdit->clear();
	});
	connect(DeleteApeCacheButton, &QPushButton::clicked, &Dialog, [this]()
	{
		const QString ApeCachePath = QDir::cleanPath(QString("%1/AssetWorks/apegdt.cache").arg(mGamePath));
		if (!QFileInfo(ApeCachePath).exists())
		{
			QMessageBox::information(this, "APE Cache", "APE cache file was not found.");
			return;
		}

		if (QFile::remove(ApeCachePath))
			QMessageBox::information(this, "APE Cache", "APE cache deleted successfully.");
		else
			QMessageBox::warning(this, "APE Cache", QString("Unable to delete '%1'.").arg(QDir::toNativeSeparators(ApeCachePath)));
	});
	connect(AssetBackgroundOpacitySlider, &QSlider::valueChanged, AssetBackgroundOpacity, &QSpinBox::setValue);
	connect(AssetBackgroundOpacity, QOverload<int>::of(&QSpinBox::valueChanged), AssetBackgroundOpacitySlider, &QSlider::setValue);
	connect(LogBackgroundOpacitySlider, &QSlider::valueChanged, LogBackgroundOpacity, &QSpinBox::setValue);
	connect(LogBackgroundOpacity, QOverload<int>::of(&QSpinBox::valueChanged), LogBackgroundOpacitySlider, &QSlider::setValue);
	connect(AssetBackgroundEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(AssetBackgroundPreview, Text);
	});
	connect(LogBackgroundEdit, &QLineEdit::textChanged, &Dialog, [=](const QString& Text)
	{
		UpdateBackgroundPreviewLabel(LogBackgroundPreview, Text);
	});
	{
		const QColor PreviewColor(AccentColorEdit->text().trimmed());
		const QString SafeColor = PreviewColor.isValid() ? PreviewColor.name(QColor::HexRgb) : QString("#444444");
		AccentBrowseButton->setStyleSheet(QString("background:%1; border:1px solid #6a6a6a; border-radius:6px;").arg(SafeColor));
	}
	UpdateBackgroundPreviewLabel(AssetBackgroundPreview, AssetBackgroundEdit->text());
	UpdateBackgroundPreviewLabel(LogBackgroundPreview, LogBackgroundEdit->text());

	auto CollectThemeValues = [&]() -> QVariantMap
	{
		QVariantMap Values;
		Values.insert("ThemeMode", ThemeModeToSettingsValue(BaseThemeCombo->currentData().toInt()));
		Values.insert("AccentColor", AccentColorEdit->text().trimmed());
		Values.insert("ShowItemTypeTags", ShowItemTypeTags->isChecked());
		Values.insert("CustomStylesheet", CustomCssEdit->toPlainText());
		Values.insert("ConsoleStyle", ImprovedConsoleStyle->isChecked() ? "improved" : "original");
		Values.insert("AssetTreeBackgroundImage", AssetBackgroundEdit->text().trimmed());
		Values.insert("AssetTreeBackgroundOpacity", AssetBackgroundOpacity->value());
		Values.insert("LogBackgroundImage", LogBackgroundEdit->text().trimmed());
		Values.insert("LogBackgroundOpacity", LogBackgroundOpacity->value());
		QString SelectedLayoutKey = SavedLayoutKey;
		for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
		{
			const LayoutOption Option = LayoutOptions[LayoutIdx];
			if (LayoutButtons.contains(Option.Key) && LayoutButtons.value(Option.Key)->isChecked())
			{
				SelectedLayoutKey = Option.Key;
				break;
			}
		}
		Values.insert("LauncherLayout", SelectedLayoutKey);
		for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
		{
			const LogColorField& Field = LogColorFields[FieldIdx];
			const QString NormalizedColor = NormalizedStoredColor(LogColorEdits.value(Field.Key)->text());
			Values.insert(Field.Key, NormalizedColor.isEmpty() ? Field.DefaultValue : NormalizedColor);
		}
		return Values;
	};

	auto LoadThemeValuesIntoControls = [&](const QVariantMap& Values)
	{
		const QString ThemeModeSetting = Values.value("ThemeMode", "original-updated").toString();
		int ThemeModeValueFromTheme = ThemeOriginalUpdated;
		if (ThemeModeSetting == "original-classic")
			ThemeModeValueFromTheme = ThemeOriginalClassic;
		else if (ThemeModeSetting == "dark-modern")
			ThemeModeValueFromTheme = ThemeDarkModern;
		BaseThemeCombo->setCurrentIndex(qMax(0, BaseThemeCombo->findData(ThemeModeValueFromTheme)));
		AccentColorEdit->setText(Values.value("AccentColor", "#ff8a2a").toString());
		ShowItemTypeTags->setChecked(Values.value("ShowItemTypeTags", true).toBool());
		CustomCssEdit->setPlainText(Values.value("CustomStylesheet", "").toString());
		if (Values.value("ConsoleStyle", "improved").toString() == "original")
			OriginalConsoleStyle->setChecked(true);
		else
			ImprovedConsoleStyle->setChecked(true);
		AssetBackgroundEdit->setText(Values.value("AssetTreeBackgroundImage", "").toString());
		AssetBackgroundOpacity->setValue(Values.value("AssetTreeBackgroundOpacity", 100).toInt());
		LogBackgroundEdit->setText(Values.value("LogBackgroundImage", "").toString());
		LogBackgroundOpacity->setValue(Values.value("LogBackgroundOpacity", 100).toInt());
		const QString ThemeLayoutKey = Values.value("LauncherLayout", "original").toString().trimmed().toLower();
		for (int LayoutIdx = 0; LayoutIdx < static_cast<int>(sizeof(LayoutOptions) / sizeof(LayoutOptions[0])); LayoutIdx++)
		{
			const LayoutOption Option = LayoutOptions[LayoutIdx];
			if (LayoutButtons.contains(Option.Key))
				LayoutButtons.value(Option.Key)->setChecked(Option.Key == ThemeLayoutKey);
		}
		for (int FieldIdx = 0; FieldIdx < ARRAYSIZE(LogColorFields); FieldIdx++)
		{
			const LogColorField& Field = LogColorFields[FieldIdx];
			LogColorEdits.value(Field.Key)->setText(Values.value(Field.Key, Field.DefaultValue).toString());
		}
	};

	auto ApplyThemeValuesPreview = [&](const QString& ThemeProfileId)
	{
		const QVariantMap Values = CollectThemeValues();
		QSettings PreviewSettings;
		for (auto It = Values.constBegin(); It != Values.constEnd(); ++It)
			PreviewSettings.setValue(It.key(), It.value());
		PreviewSettings.setValue(kThemeProfileSettingKey, ThemeProfileId);
		mThemeProfileId = ThemeProfileId;
		mThemeMode = ThemeModeFromSettings(PreviewSettings);
		mLauncherLayout = PreviewSettings.value("LauncherLayout", mLauncherLayout).toString().trimmed().toLower();
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
	};

	auto RefreshThemeButtons = [&]()
	{
		const QString SelectedThemeProfileId = ThemeProfileCombo->currentData().toString();
		const bool IsBuiltInTheme = SelectedThemeProfileId == "original-updated" || SelectedThemeProfileId == "original-classic" || SelectedThemeProfileId == "dark-modern";
		DeleteThemeButton->setEnabled(!IsBuiltInTheme);
		SaveThemeButton->setText(IsBuiltInTheme ? "Update Built-in" : "Save Theme");
	};

	connect(ThemeProfileCombo, &QComboBox::currentIndexChanged, &Dialog, [=](int)
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		LoadThemeValuesIntoControls(ThemeProfileValues(ThemeProfileId));
		ApplyThemeProfile(ThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
		RefreshThemeButtons();
	});
	connect(BaseThemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &Dialog, [=](int)
	{
		ApplyThemeValuesPreview(ThemeProfileCombo->currentData().toString());
	});
	connect(SaveThemeButton, &QPushButton::clicked, &Dialog, [=]()
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		SaveThemeProfile(ThemeProfileId, ThemeProfileDisplayName(ThemeProfileId), CollectThemeValues());
		ApplyThemeProfile(ThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
	});
	connect(SaveThemeAsButton, &QPushButton::clicked, &Dialog, [=, &Dialog]()
	{
		bool Accepted = false;
		const QString ThemeName = QInputDialog::getText(&Dialog, "Save Theme As", "Theme name:", QLineEdit::Normal, "", &Accepted).trimmed();
		if (!Accepted || ThemeName.isEmpty())
			return;
		QString ThemeProfileId = SanitizedThemeProfileId(ThemeName);
		QString UniqueThemeProfileId = ThemeProfileId;
		int Suffix = 2;
		while (AvailableThemeProfileIds().contains(UniqueThemeProfileId))
			UniqueThemeProfileId = QString("%1-%2").arg(ThemeProfileId).arg(Suffix++);
		SaveThemeProfile(UniqueThemeProfileId, ThemeName, CollectThemeValues());
		ThemeProfileCombo->addItem(ThemeName, UniqueThemeProfileId);
		ThemeProfileCombo->setCurrentIndex(ThemeProfileCombo->findData(UniqueThemeProfileId));
		ApplyThemeProfile(UniqueThemeProfileId);
		ApplyLauncherLayout();
		UpdateTheme();
		PopulateFileList();
		RefreshThemeButtons();
	});
	connect(DeleteThemeButton, &QPushButton::clicked, &Dialog, [=, &Dialog]()
	{
		const QString ThemeProfileId = ThemeProfileCombo->currentData().toString();
		if (ThemeProfileId == "original-updated" || ThemeProfileId == "original-classic" || ThemeProfileId == "dark-modern")
			return;
		if (QMessageBox::question(&Dialog, "Delete Theme", QString("Delete theme '%1'?").arg(ThemeProfileDisplayName(ThemeProfileId))) != QMessageBox::Yes)
			return;
		QSettings DeleteSettings;
		DeleteSettings.beginGroup(kThemeProfilesGroup);
		DeleteSettings.remove(ThemeProfileId);
		DeleteSettings.endGroup();
		const int RemoveIndex = ThemeProfileCombo->currentIndex();
		ThemeProfileCombo->removeItem(RemoveIndex);
		const int FallbackIndex = qMax(0, ThemeProfileCombo->findData(QString("original-updated")));
		ThemeProfileCombo->setCurrentIndex(FallbackIndex);
	});
	RefreshThemeButtons();

	QTimer* CssApplyTimer = new QTimer(&Dialog);
	CssApplyTimer->setSingleShot(true);
	auto ApplyCustomCssPreview = [=]()
	{
		ApplyThemeValuesPreview(ThemeProfileCombo->currentData().toString());
	};
	connect(CustomCssEdit, &QPlainTextEdit::textChanged, &Dialog, [=]()
	{
		CssApplyTimer->start(200);
	});
	connect(CssApplyTimer, &QTimer::timeout, &Dialog, ApplyCustomCssPreview);
	connect(ApplyCustomCssButton, &QPushButton::clicked, &Dialog, [=]()
	{
		CssApplyTimer->stop();
		ApplyCustomCssPreview();
	});

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mBuildLanguage = LanguageCombo->currentText();
	const QString SelectedThemeProfileId = ThemeProfileCombo->currentData().toString();
	const QVariantMap SelectedThemeValues = CollectThemeValues();

	Settings.setValue("BuildLanguage", mBuildLanguage);
	Settings.setValue("ShowRecents", ShowRecentsCheckbox->isChecked());
	Settings.setValue("ShowFavorites", ShowFavoritesCheckbox->isChecked());
	SaveThemeProfile(SelectedThemeProfileId, ThemeProfileDisplayName(SelectedThemeProfileId), SelectedThemeValues);
	ApplyThemeProfile(SelectedThemeProfileId);

	ApplyLauncherLayout();
	UpdateTheme();
	PopulateFileList();
}

void mlMainWindow::UpdateTheme()
{
	QSettings Settings;
	mThemeProfileId = CurrentThemeProfileId();
	QString AccentColor = Settings.value("AccentColor", "#ff8a2a").toString().trimmed();
	if (!QColor(AccentColor).isValid())
		AccentColor = "#ff8a2a";
	const QString AssetTreeBackgroundImage = Settings.value("AssetTreeBackgroundImage", "").toString().trimmed();
	const int AssetTreeBackgroundOpacity = Settings.value("AssetTreeBackgroundOpacity", 100).toInt();
	const QString LogBackgroundImage = Settings.value("LogBackgroundImage", "").toString().trimmed();
	const int LogBackgroundOpacity = Settings.value("LogBackgroundOpacity", 100).toInt();
	const QString CustomStylesheet = Settings.value("CustomStylesheet", "").toString();
	const QString DefaultLogColor = Settings.value("LogColors/Default", "#d7dce2").toString().trimmed();
	const QString AccentHoverLight = QColor(AccentColor).lighter(115).name(QColor::HexRgb);
	const QString AccentHoverDark = AccentColor;
	const QString AssetTreeCachedBackground = PrepareBackgroundImageCache(LauncherDataRoot(), AssetTreeBackgroundImage, AssetTreeBackgroundOpacity);
	mAssetTreeBackgroundCachePath = AssetTreeCachedBackground;
	const QString LogCachedBackground = PrepareBackgroundImageCache(LauncherDataRoot(), LogBackgroundImage, LogBackgroundOpacity);
	mLogBackgroundCachePath = LogCachedBackground;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool UseClassicChrome = ThemeUsesClassicChrome(mThemeMode);
	const bool UseUpdatedChrome = ThemeUsesUpdatedChrome(mThemeMode);
	const bool UseDarkModernChrome = ThemeUsesDarkModernChrome(mThemeMode);
	setProperty("themeProfile", mThemeProfileId);
	UpdateOutputConsoleMode();
	if (mLogFiltersButton)
		mLogFiltersButton->setVisible(ImprovedConsole);
	if (mLogSelectionButton)
		mLogSelectionButton->setVisible(ImprovedConsole);

	if (UseClassicChrome)
	{
		const QString ClassicConsoleBackground = QApplication::palette().color(QPalette::Base).name(QColor::HexRgb);
		qApp->setStyle("Windows");
		QFile file(QString("%1/radiant/stylesheet.qss").arg(mToolsPath));
		file.open(QFile::ReadOnly);
		QString styleSheet = QLatin1String(file.readAll());
		file.close();
			styleSheet += QString(
				"#AssetListPanel { background: transparent; }"
				"#ItemRowWidget { background: #171b20; }"
				"#ItemRowWidget { background: transparent; }"
				"#AssetTree { margin-top: 0; padding: 0; background: #5a5a5a; }"
				"#AssetTree::viewport { background: #5a5a5a; }"
				"#OutputConsole, #OutputConsolePlain { background: %1; }"
				"#AssetTree::item { min-height: 16px; padding: 0; margin: 0; color: transparent; }"
			"#AssetTree::item:hover { color: transparent; }"
			"#AssetTree::item:selected { color: transparent; }"
			"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
			"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
			"#OutputConsole::branch { width: 0px; background: transparent; }"
				"#ItemTitleWidget, #QuickActionStrip, #ItemInternalNameLabel, #ItemTypeTag { background: transparent; border: 0; }"
				"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"], #ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: transparent; }"
				"#ItemNameButton { background: transparent; border: 0; padding: 0; text-align: left; color: #b1b1b1; font-weight: 400; }"
				"#ItemInternalNameLabel, #ItemTypeTag { color: #9a9a9a; padding: 0; margin-left: 2px; }"
				"#QuickLaunchCombo { min-height: 28px; max-height: 28px; padding: 2px 28px 2px 8px; }"
				"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
				"QCheckBox::indicator { width: 12px; height: 12px; border-radius: 1px; border: 1px solid #7a7a7a; background: #5f5f5f; }"
				"#ItemSelectCheckBox::indicator { width: 12px; height: 12px; margin: 0; border-radius: 1px; border: 1px solid #808080; background: #666666; }"
				"#QuickActionStrip { min-height: 22px; }"
				"#QuickActionButton { min-height: 18px; padding: 0 4px; }"
				"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; min-height: 22px; color: #b1b1b1; }"
				"#DisplayNameAddButton { min-height: 18px; padding: 0; }"
			"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; min-height: 18px; color: #d8d8d8; font-size: 11px; }"
			"#BackgroundPreview { border: 1px solid #5a5a5a; background: #2a2a2a; color: #cfcfcf; }"
			"#GameStateLabel { color: #7a7a7a; padding-left: 2px; }"
			"#FooterStatusLabel { color: #d8d8d8; padding: 4px 8px; font-weight: 600; }")
			.arg(ClassicConsoleBackground)
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
			+ "#ItemNameButton:hover { background: transparent; color: #111111; }"
			+ "#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag { color: #111111; }"
			+ "#ItemTitleWidget[selected=\"true\"] #ItemNameButton, #ItemTitleWidget[selected=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[selected=\"true\"] #ItemTypeTag { color: #111111; }"
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickMiniActionButton:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ "#WorkshopVersionsList { border: 1px solid #5a5a5a; background: #262626; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 10px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ "#WorkshopVersionsList::item:hover { background: #3a3a3a; border-color: #707070; }"
			+ "#WorkshopVersionsList::item:selected { background: #4a4a4a; border-color: #8d8d8d; }"
			+ "#WorkshopVersionRow { background: #303030; border: 1px solid #444444; border-radius: 8px; }"
			+ "#WorkshopVersionRowActive { background: #3b3f2d; border: 1px solid #8cb667; border-radius: 8px; }"
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ "#WorkshopVersionActiveBadge { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 8px; padding: 2px 8px; font-weight: 700; }"
			+ CustomStylesheet;
		qApp->setStyleSheet(styleSheet);
	}
	else if (UseUpdatedChrome)
	{
		qApp->setStyle("Windows");
		QFile file(QString("%1/radiant/stylesheet.qss").arg(mToolsPath));
		file.open(QFile::ReadOnly);
		QString styleSheet = QLatin1String(file.readAll());
		file.close();
		styleSheet += QString(
			"QPushButton { padding: 6px 10px; border-radius: 4px; }"
					"#AssetListPanel { background: transparent; }"
					"#ItemRowWidget { background: transparent; }"
					"#SettingsNavList { background: #232323; border: 1px solid #3a3a3a; padding: 4px; }"
			"#SettingsNavList::item { padding: 10px 12px; border-radius: 6px; }"
			"#SettingsNavList::item:selected { background: #3b3b3b; color: #f0f0f0; }"
			"#CategoryTabs { background: transparent; }"
			"#CategoryTabs::tab { background: #2f2f2f; border: 1px solid #1e1e1e; border-bottom: 0; padding: 8px 14px; margin: 6px 3px 0 0; min-width: 70px; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
					"#AssetTree { margin-top: 0; border-top-left-radius: 0; padding: 0; background: #4f4f4f; }"
					"#AssetTree::viewport { background: #4f4f4f; }"
				"#AssetTree::item { min-height: 26px; padding: 2px 0; margin: 4px 0; color: transparent; }"
				"#AssetTree::item:hover { background: transparent; color: transparent; }"
				"#AssetTree::item:selected { background: transparent; color: transparent; }"
				"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
				"#AssetTree::branch { background: transparent; }"
					"#OutputConsole, #OutputConsolePlain { background: #161616; }"
				"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
				"#OutputConsole::branch { width: 0px; background: transparent; }"
					"#ItemTitleWidget, #QuickActionStrip { background: #3a3a3a; border: 0; border-radius: 4px; }"
					"#ItemTitleWidget { border-right: 1px solid #454545; }"
					"#QuickActionStrip { border-left: 1px solid #454545; }"
					"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"] { background: #434343; }"
				"#ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: rgba(138, 83, 22, 168); }"
				"#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag, #QuickActionStrip[hovered=\"true\"] #QuickActionButton { color: #ffffff; }"
					"#ItemTypeTag { border-radius: 5px; padding: 1px 6px; margin-left: 2px; font-size: 10px; font-weight: 700; border: 1px solid #4d4d4d; background: #404040; color: #c9c9c9; }"
					"#ItemTypeTag[itemType=\"map\"] { background: #444444; color: #d0d0d0; }"
					"#ItemTypeTag[itemType=\"mod\"] { background: #3a3a3a; color: #c4c4c4; }"
			"#ItemNameButton { background: transparent; border: 0; padding: 0; text-align: left; color: #f0f0f0; font-weight: 600; }"
				"#ItemInternalNameLabel { color: #bcbcbc; background: #3f3f3f; border: 1px solid #575757; border-radius: 5px; padding: 1px 6px; margin-left: 2px; }"
			"#QuickLaunchCombo { min-height: 28px; max-height: 28px; padding: 2px 28px 2px 8px; }"
			"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
			"#ItemSelectCheckBox::indicator { width: 14px; height: 14px; margin: 0; }"
			"#QuickActionStrip { min-height: 34px; }"
			"#QuickActionButton { min-height: 22px; padding: 2px 6px; }"
			"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; min-height: 22px; color: #f0f0f0; }"
			"#DisplayNameAddButton { background: #3f3f3f; border: 1px solid #686868; border-radius: 6px; padding: 0; min-height: 22px; color: #f0f0f0; }"
			"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; min-height: 18px; color: #d8d8d8; font-size: 11px; }"
					"#BackgroundPreview { border: 1px solid #5a5a5a; background: #2f2f2f; color: #cfcfcf; }"
			"QComboBox, QLineEdit, QCheckBox { padding: 4px; }"
			"#SuccessBanner { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 6px; padding: 8px; }"
			"#WarningBanner { background: #6b4b24; color: #fff0de; border: 1px solid #d58b34; border-radius: 6px; padding: 8px; }"
					"#InfoBanner { background: #575757; color: #ededed; border: 1px solid #9a9a9a; border-radius: 6px; padding: 8px; }"
				"#GameStateLabel { color: #7a7a7a; padding-left: 2px; }"
					"QStatusBar { background: #2a2a2a; border-top: 1px solid #4e4e4e; }"
					"#FooterStatusLabel { color: #d8d8d8; padding: 4px 8px; font-weight: 600; }")
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
			+ QString("QPushButton:hover, QToolButton:hover { background: #474747; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverLight)
			+ QString("#CategoryTabs::tab:selected { color: #111111; background: %1; border-color: %1; }").arg(AccentHoverLight)
			+ QString("#CategoryTabs::tab:selected:hover { background: %1; border-color: %1; color: #111111; }").arg(QColor(AccentHoverLight).darker(108).name(QColor::HexRgb))
			+ QString("#CategoryTabs::tab:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("QMenuBar::item:selected, QToolBar QToolButton:hover { border: 1px solid %1; }").arg(AccentHoverLight)
			+ QString("#ItemNameButton:hover { background: transparent; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickActionButton:hover { background: #474747; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("#QuickMiniActionButton:hover { border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("#DisplayNameAddButton:hover { background: #505050; border-color: %1; color: %1; }").arg(AccentHoverLight)
			+ QString("QCheckBox::indicator:checked { background: %1; border-color: %1; image: url(:/resources/checkmark_white.svg); }").arg(AccentHoverLight)
			+ "#WorkshopVersionsList { border: 1px solid #5a5a5a; background: #262626; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 10px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ "#WorkshopVersionsList::item:hover { background: #3a3a3a; border-color: #707070; }"
			+ "#WorkshopVersionsList::item:selected { background: #4a4a4a; border-color: #8d8d8d; }"
			+ "#WorkshopVersionRow { background: #303030; border: 1px solid #444444; border-radius: 8px; }"
			+ "#WorkshopVersionRowActive { background: #3b3f2d; border: 1px solid #8cb667; border-radius: 8px; }"
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ "#WorkshopVersionActiveBadge { background: #4f6540; color: #eef6e8; border: 1px solid #8cb667; border-radius: 8px; padding: 2px 8px; font-weight: 700; }"
			+ CustomStylesheet;
		qApp->setStyleSheet(styleSheet);
	}
	else if (UseDarkModernChrome)
	{
		qApp->setStyle("Fusion");
		qApp->setStyleSheet(
			QString(
					"QMainWindow, QDialog { background: #141414; color: #eef1f4; }"
					"#SettingsNavList { background: #1a1a1a; border: 1px solid #323232; padding: 4px; }"
			"#SettingsNavList::item { padding: 10px 12px; border-radius: 8px; }"
					"#SettingsNavList::item:selected { background: #303030; color: #eef1f4; }"
			"#AssetListPanel { background: transparent; }"
			"#CategoryTabs { background: transparent; }"
					"#CategoryTabs::tab { background: #202020; color: #a8a8a8; border: 1px solid #363636; border-bottom: 0; padding: 9px 15px; margin: 8px 4px 0 0; min-width: 72px; border-top-left-radius: 10px; border-top-right-radius: 10px; }"
					"QMenuBar, QToolBar { background: #1b1b1b; border: 0; spacing: 6px; padding: 8px; }"
			"QMenuBar::item { background: transparent; padding: 6px 10px; border-radius: 8px; }"
					"QMenuBar::item:selected, QMenu::item:selected { background: #303030; }"
					"QMenu { background: #1b1b1b; border: 1px solid #363636; padding: 6px; }"
			"QMenu::item { padding: 8px 22px; border-radius: 8px; }"
			"QMenu::indicator { width: 14px; height: 14px; }"
					"QSplitter::handle { background: #262626; }"
					"#ActionsPanel { background: #1b1b1b; border: 1px solid #363636; border-radius: 12px; }"
			"#AssetTree, #OutputConsole, QTextEdit, QTextBrowser, QLineEdit, QComboBox, QTreeWidget, QPlainTextEdit {"
					" background: #1b1b1b; border: 1px solid #363636; border-radius: 12px; color: #eef1f4; padding: 6px; selection-background-color: #303030; selection-color: #ffffff; }"
						"#AssetTree { margin-top: 0; border-top-left-radius: 0; padding: 0; background: #1b1b1b; }"
						"#AssetTree::viewport { background: #1b1b1b; }"
			"#AssetTree { show-decoration-selected: 0; }"
			"#AssetTree::branch, #AssetTree::branch:selected { background: transparent; }"
						"#AssetTree::item { padding: 2px 0; margin: 4px 0; border-radius: 10px; background: transparent; color: transparent; }"
						"#AssetTree::item:hover { background: transparent; color: transparent; }"
						"#AssetTree::item:selected { background: transparent; color: transparent; }"
			"#AssetTree::indicator { width: 0px; height: 0px; margin: 0px; }"
					"#OutputConsole, #OutputConsolePlain { background: #181818; }"
			"#OutputConsole::item { padding: 0; margin: 0; border: 0; background: transparent; }"
			"#OutputConsole::branch { width: 0px; background: transparent; }"
						"#ItemRowWidget { background: transparent; }"
						"#ItemTitleWidget, #QuickActionStrip { background: #2d2d2d; border: 0; border-radius: 10px; }"
						"#ItemTitleWidget { border-right: 1px solid #3b3b3b; }"
						"#QuickActionStrip { border-left: 1px solid #3b3b3b; }"
						"#ItemTitleWidget[childRow=\"true\"], #QuickActionStrip[childRow=\"true\"] { background: #383838; }"
						"#ItemTitleWidget[hovered=\"true\"], #QuickActionStrip[hovered=\"true\"] { background: #494949; }"
			"#ItemTitleWidget[hovered=\"true\"] #ItemNameButton, #ItemTitleWidget[hovered=\"true\"] #ItemInternalNameLabel, #ItemTitleWidget[hovered=\"true\"] #ItemTypeTag, #QuickActionStrip[hovered=\"true\"] #QuickActionButton { color: #ffffff; }"
			"#ItemTypeTag { border-radius: 5px; padding: 1px 6px; margin-left: 2px; font-size: 10px; font-weight: 700; border: 1px solid #1f1f1f; background: #252525; color: #8b8b8b; }"
			"#ItemTypeTag[itemType=\"map\"] { background: #22272b; color: #8c98a2; }"
			"#ItemTypeTag[itemType=\"mod\"] { background: #2a2320; color: #a08a7a; }"
			"#ItemNameButton { background: transparent; border: 0; padding: 0; color: #eef1f4; text-align: left; font-weight: 600; }"
			"#ItemInternalNameLabel { color: #cfcfcf; background: #232323; border: 1px solid #2f2f2f; border-radius: 5px; padding: 1px 6px; margin-left: 4px; }"
			"QPushButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 8px; padding: 10px 16px; color: #eef1f4; min-height: 18px; }"
			"#ActionsPanel QPushButton { padding: 7px 16px; min-height: 14px; }"
			"QPushButton:disabled { color: #7f8993; background: #1b1b1b; border-color: #363636; }"
			"#QuickActionStrip { min-height: 34px; }"
			"#QuickActionButton { background: #222222; border: 1px solid #3a3a3a; border-radius: 5px; padding: 2px 6px; min-height: 22px; }"
			"#QuickActionButton:pressed { background: #343434; }"
			"#QuickMiniActionButton { background: transparent; border: 0; padding: 0; color: #eef1f4; min-height: 22px; }"
			"#DisplayNameAddButton { background: #242424; border: 1px solid #3a3a3a; border-radius: 6px; padding: 0; color: #eef1f4; min-height: 22px; }"
			"#LogFiltersButton { background: transparent; border: 0; padding: 0 8px; color: #cbd2d9; min-height: 18px; font-size: 11px; }"
			"#QuickMiniActionButton:checked { background: #232323; border-color: #5a5a5a; color: #ffffff; }"
			"#BackgroundPreview { border: 1px solid #3a3a3a; background: #1b1b1b; color: #a8a8a8; }"
			"QCheckBox, QLabel { color: #eef1f4; }"
			"QCheckBox::indicator { width: 18px; height: 18px; border-radius: 6px; border: 1px solid #5d5d5d; background: #1d1d1d; }"
			"#ItemSelectCheckBox { spacing: 0; padding: 0; margin: 0; }"
			"#ItemSelectCheckBox::indicator { width: 14px; height: 14px; border-radius: 4px; margin: 0; }"
			"QComboBox { padding: 6px 28px 6px 10px; }"
			"#QuickLaunchCombo { min-height: 26px; max-height: 26px; padding: 1px 28px 1px 10px; }"
			"QComboBox::drop-down { border: 0; width: 26px; subcontrol-origin: padding; subcontrol-position: top right; }"
			"QComboBox::down-arrow { image: url(:/stylesheet/stylesheet/down_arrow.png); width: 10px; height: 10px; }"
			"QScrollBar:vertical { background: #151515; width: 12px; margin: 8px 0; }"
			"QScrollBar::handle:vertical { background: #424242; min-height: 28px; border-radius: 6px; }"
			"QHeaderView::section { background: #1b1b1b; color: #eef1f4; border: 0; padding: 6px; }"
			"#WarningBanner { background: #2b1f17; border: 1px solid #ff8a2a; border-radius: 12px; padding: 12px; font-weight: 700; }"
			"#SuccessBanner { background: #15241a; border: 1px solid #44d17a; border-radius: 12px; padding: 10px; }"
			"#InfoBanner { background: #232323; border: 1px solid #3a3a3a; border-radius: 12px; padding: 10px; }"
			"#GameStateLabel { color: #8d96a0; padding-left: 2px; }"
			"QStatusBar { background: #1b1b1b; border-top: 1px solid #363636; }"
			"#FooterStatusLabel { color: #cfcfcf; padding: 4px 8px; font-weight: 600; }")
			+ QString("#OutputConsole, #OutputConsolePlain { color: %1; }").arg(DefaultLogColor)
			+ QString("#CategoryTabs::tab:selected { color: #ffffff; background: %1; border-color: %1; }").arg(AccentHoverDark)
			+ QString("#CategoryTabs::tab:selected:hover { color: #ffffff; background: %1; border-color: %1; }").arg(QColor(AccentHoverDark).darker(112).name(QColor::HexRgb))
			+ QString("#CategoryTabs::tab:hover { color: #eef1f4; border-color: %1; }").arg(AccentHoverDark)
			+ QString("#ItemNameButton:hover { background: transparent; color: %1; }").arg(AccentHoverDark)
			+ QString("QPushButton:hover { background: #303030; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#LogFiltersButton:hover { background: transparent; border: 0; color: %1; }").arg(AccentHoverDark)
			+ QString("#BuildButton, #BuildEnglishButton { background: #20262d; border-color: %1; color: #ffffff; font-weight: 700; }").arg(AccentHoverDark)
			+ "#BuildButton:disabled, #BuildEnglishButton:disabled { background: #1b1b1b; border-color: #363636; color: #7f8993; }"
			+ QString("#QuickActionButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#QuickMiniActionButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("#DisplayNameAddButton:hover { background: #343434; border-color: %1; color: %1; }").arg(AccentHoverDark)
			+ QString("QCheckBox::indicator:checked { background: %1; border-color: %1; image: url(:/resources/checkmark_white.svg); }").arg(AccentHoverDark)
			+ "#WorkshopVersionsList { border: 1px solid #2d353d; border-radius: 12px; background: #171b20; outline: 0; }"
			+ "#WorkshopVersionsList::item { border: 1px solid transparent; border-radius: 12px; margin: 3px 2px; padding: 2px; background: transparent; outline: 0; }"
			+ QString("#WorkshopVersionsList::item:hover { background: #222a31; border-color: %1; }").arg(AccentHoverDark)
			+ QString("#WorkshopVersionsList::item:selected { background: #252e36; border-color: %1; }").arg(AccentHoverDark)
			+ "#WorkshopVersionRow { background: #1d2329; border: 1px solid #2d353d; border-radius: 10px; }"
			+ QString("#WorkshopVersionRowActive { background: #1f2d23; border: 1px solid %1; border-radius: 10px; }").arg(AccentHoverDark)
			+ "#WorkshopVersionTitleLabel, #WorkshopVersionIdLabel { background: transparent; border: 0; }"
			+ QString("#WorkshopVersionActiveBadge { background: %1; color: #ffffff; border: 1px solid %1; border-radius: 8px; padding: 2px 8px; font-weight: 700; }").arg(AccentHoverDark)
			+ CustomStylesheet
		);
	}
	UpdateThemeMenuChecks();
	UpdateBackgroundOverlays();
	if (mQuickLaunchWidget)
	{
		if (ThemeUsesDarkModernChrome(mThemeMode))
		{
			mQuickLaunchWidget->setMinimumHeight(26);
			mQuickLaunchWidget->setMaximumHeight(26);
			if (mQuickLaunchWidget->view())
				mQuickLaunchWidget->view()->setMaximumHeight(260);
		}
		else
		{
			mQuickLaunchWidget->setMinimumHeight(28);
			mQuickLaunchWidget->setMaximumHeight(28);
			if (mQuickLaunchWidget->view())
				mQuickLaunchWidget->view()->setMaximumHeight(320);
		}
	}
}

void mlMainWindow::OnEditDvars()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Dvar Options");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* Label = new QLabel(&Dialog);
	Label->setText("Dvars that are to be used when you run the game.\nMust press \"OK\" in order to save the values!");
	Layout->addWidget(Label);

	QTreeWidget* DvarTree = new QTreeWidget(&Dialog);
	DvarTree->setColumnCount(2);
	DvarTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	DvarTree->setHeaderLabels(QStringList() << "Dvar" << "Value");
	DvarTree->setUniformRowHeights(true);
	DvarTree->setRootIsDecorated(false);
	Layout->addWidget(DvarTree);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	for(int DvarIdx = 0; DvarIdx < ARRAYSIZE(gDvars); DvarIdx++)
		Dvar(gDvars[DvarIdx], DvarTree);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	int size = 0;
	QTreeWidgetItemIterator it(DvarTree);

	mRunDvars.clear();
	while (*it && size < ARRAYSIZE(gDvars))
	{
		QWidget* widget = DvarTree->itemWidget(*it, 1);
		QString dvarName = (*it)->data(0, 0).toString();
		QString dvarValue;
		dvar_s dvar = Dvar::findDvar(dvarName, DvarTree, gDvars, ARRAYSIZE(gDvars));
		switch(dvar.type)
		{
		case DVAR_VALUE_BOOL:
			dvarValue = Dvar::setDvarSetting(dvar, (QCheckBox*)widget);
			break;
		case DVAR_VALUE_INT:
			dvarValue = Dvar::setDvarSetting(dvar, (QSpinBox*)widget);
			break;
		case DVAR_VALUE_STRING:
			dvarValue = Dvar::setDvarSetting(dvar, (QLineEdit*)widget);
			break;
		}

		if(!dvarValue.toLatin1().isEmpty())
		{
			if(!dvar.isCmd)
				mRunDvars << "+set" << dvarName;
			else			// hack for cmds
				mRunDvars << QString("+%1").arg(dvarName);
			mRunDvars << dvarValue;
		}
		size++;
		++it;
	}
}

bool mlMainWindow::SaveWorkshopMetadata()
{
	QJsonObject Root;

	if (mFileId)
		Root["PublisherID"] = QString::number(mFileId);
	Root["Title"] = mTitle;
	Root["Description"] = mBriefingDescription;
	Root["SteamDescription"] = mSteamDescription;
	Root["Thumbnail"] = mThumbnail;
	Root["Type"] = mType;
	Root["FolderName"] = mFolderName;
	Root["Tags"] = mTags.join(',');

	QString WorkshopFile(mWorkshopFolder + "/workshop.json");
	QFile File(WorkshopFile);

	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Error", QString("Error writing to file '%1'.").arg(WorkshopFile));
		return false;
	}

	File.write(QJsonDocument(Root).toJson());
	File.close();
	return true;
}

void mlMainWindow::UpdateWorkshopItem(bool UploadToWorkshop)
{
	if (UploadToWorkshop)
	{
		const QString ContentRoot = QFileInfo(mWorkshopFolder).dir().absolutePath();
		if (HasOnlyEnglishBuild(ContentRoot))
		{
			if (QMessageBox::warning(this, "English Only Build Detected", "Only English localized build output was detected for this item. Uploading now may ship English-only Workshop files. Continue anyway?", QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return;
		}
	}

	if (!SaveWorkshopMetadata())
		return;

	if (!mFileId)
		return;

	UGCUpdateHandle_t UpdateHandle = SteamUGC()->StartItemUpdate(AppId, mFileId);
	const QString SubmittedTitle = UploadToWorkshop ? mTitle : StripTreyarchColorCodes(mTitle);
	const QString SubmittedDescription = UploadToWorkshop ? mBriefingDescription : SteamDescriptionForUpload(mBriefingDescription, mSteamDescription);
	SteamUGC()->SetItemTitle(UpdateHandle, SubmittedTitle.toLatin1().constData());
	SteamUGC()->SetItemDescription(UpdateHandle, SubmittedDescription.toLatin1().constData());
	SteamUGC()->SetItemPreview(UpdateHandle, mThumbnail.toLatin1().constData());

	const char* TagList[ARRAYSIZE(gTags)];
	SteamParamStringArray_t Tags;
	Tags.m_ppStrings = TagList;
	Tags.m_nNumStrings = 0;

	for (const QString& Tag : mTags)
	{
		QByteArray TagStr = Tag.toLatin1();

		for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
		{
			if (TagStr == gTags[TagIdx])
			{
				TagList[Tags.m_nNumStrings++] = gTags[TagIdx];
				if (Tags.m_nNumStrings == ARRAYSIZE(TagList))
					break;
			}
		}
	}

	SteamUGC()->SetItemTags(UpdateHandle, &Tags);

	if (!UploadToWorkshop)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->SubmitItemUpdate(UpdateHandle, "");
		mSteamCallResultUpdateItem.Set(SteamAPICall, this, &mlMainWindow::OnUpdateItemResult);
		return;
	}

	SteamUGC()->SetItemContent(UpdateHandle, mWorkshopFolder.toLatin1().constData());

	SteamAPICall_t SteamAPICall = SteamUGC()->SubmitItemUpdate(UpdateHandle, "");
	mSteamCallResultUpdateItem.Set(SteamAPICall, this, &mlMainWindow::OnUpdateItemResult);

	QProgressDialog Dialog(this);
	Dialog.setLabelText(QString("Uploading workshop item '%1'...").arg(QString::number(mFileId)));
	Dialog.setCancelButton(NULL);
	Dialog.setWindowModality(Qt::WindowModal);
	Dialog.resize(720, 160);
	if (ThemeUsesDarkModernChrome(mThemeMode))
	{
		Dialog.setStyleSheet(
			"QProgressDialog { background: #111418; color: #eef1f4; }"
			"QLabel { color: #eef1f4; font-weight: 600; }"
			"QProgressBar { background: #1a2026; border: 1px solid #2d353d; border-radius: 10px; text-align: center; min-height: 22px; color: #eef1f4; }"
			"QProgressBar::chunk { background: #ff8a2a; border-radius: 8px; margin: 2px; }");
	}
	Dialog.show();

	for (;;)
	{
		uint64 Processed, Total;

		const auto Status = SteamUGC()->GetItemUpdateProgress(SteamAPICall, &Processed, &Total);
		// if we get an invalid status exit out, it could mean we're finished or there's an actual problem
		if (Status == k_EItemUpdateStatusInvalid)
		{
			break;
		}

		switch (Status)
		{
		case EItemUpdateStatus::k_EItemUpdateStatusInvalid:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Invalid" )));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingConfig:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Config")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingPreviewFile:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Preview file")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusCommittingChanges:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Committing Changes")));
			break;
		}

		Dialog.setMaximum(Total);
		Dialog.setValue(Processed);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		Sleep(100);
	}
}

void mlMainWindow::OnCreateItemResult(CreateItemResult_t* CreateItemResult, bool IOFailure)
{
	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (CreateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error creating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(CreateItemResult->m_eResult));
		return;
	}

	mFileId = CreateItemResult->m_nPublishedFileId;

	UpdateWorkshopItem(true);
}

void mlMainWindow::OnUpdateItemResult(SubmitItemUpdateResult_t* UpdateItemResult, bool IOFailure)
{
	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (UpdateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error updating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(UpdateItemResult->m_eResult));
		return;
	}

	if (mPostUploadSteamSyncPending)
	{
		mPostUploadSteamSyncPending = false;
		UpdateWorkshopItem(false);
		return;
	}

	if (QMessageBox::question(this, "Update", "Workshop item successfully updated. Do you want to visit the Workshop page for this item now?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
		ShellExecute(NULL, "open", QString("steam://url/CommunityFilePage/%1").arg(QString::number(mFileId)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
}

void mlMainWindow::OnHelpAbout()
{
	QMessageBox::about(this, "About BO3 Mod Tools Black",
		"BO3 Mod Tools Black\n\n"
		"Original by Treyarch\n"
		"Edits by Sphynx\n\n"
		"Launcher features include:\n"
		"- Tabbed map/mod browsing\n"
		"- Favorites and recents\n"
		"- Display names and notes\n"
		"- Workshop version switching\n"
		"- Background previews and theme options\n"
		"- Ready for Publish workflow helpers");
}

void mlMainWindow::OnOpenZoneFile()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;
	
	QTreeWidgetItem* Item = ItemList[0];

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = GetItemEntryName(Item);
		ShellExecute(NULL, "open", QString("\"%1/usermaps/%2/zone_source/%3.zone\"").arg(mGamePath, MapName, MapName).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		QString ZoneName = GetItemEntryName(Item);
		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MOD_GROUP)
			return;
		ShellExecute(NULL, "open", (QString("\"%1/mods/%2/zone_source/%3.zone\"").arg(mGamePath, ModName, ZoneName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
}

void mlMainWindow::OnOpenModRootFolder()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = GetItemEntryName(Item);
		ShellExecute(NULL, "open", (QString("\"%1/usermaps/%2\"").arg(mGamePath, MapName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		ShellExecute(NULL, "open", (QString("\"%1/mods/%2\"").arg(mGamePath, ModName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
}

void mlMainWindow::OnRunMapOrMod()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString FsGame;
	QString MapName;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		MapName = GetItemEntryName(Item);
		TouchRecentEntry(RecentEntryForItem(ML_ITEM_MAP, MapName, MapName));
		FsGame = MapName;
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		TouchRecentEntry(RecentEntryForItem(Item->data(0, Qt::UserRole).toInt(), ModName, GetItemEntryName(Item)));
		FsGame = ModName;
	}

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(CreateGameLaunchCommand(FsGame, MapName));
	StartBuildThread(Commands);
}

void mlMainWindow::OnSaveLog() const
{
	// want to make a logs directory for easy management of launcher logs (exe_dir/logs)
	const auto dir = QDir{};
	if (!dir.exists("logs"))
	{
		const auto result = dir.mkdir("logs");
		if (!result)
		{
			QMessageBox::warning(nullptr, "Error", QString("Could not create the \"logs\" directory"));
			return;
		}
	}

	const auto time = std::time(nullptr);
	auto ss = std::stringstream{};
	const auto timeStr = std::put_time(std::localtime(&time), "%F_%T");

	ss << timeStr;

	auto dateStr = ss.str();
	std::replace(dateStr.begin(), dateStr.end(), ':', '_');

	QFile log(QString("logs/modlog_%1.txt").arg(dateStr.c_str()));

	if (!log.open(QIODevice::WriteOnly))
		return;

	QTextStream stream(&log);
	if (!mOutputFullText.isEmpty())
		stream << mOutputFullText;
	else if (mOutputPlainWidget && mOutputPlainWidget->isVisible())
		stream << mOutputPlainWidget->toPlainText();
	else
	{
		std::function<void(const QTreeWidgetItem*)> WriteItem = [&](const QTreeWidgetItem* Item)
		{
			if (!Item)
				return;

			const QString Line = Item->data(0, ML_LOG_TEXT_ROLE).toString();
			if (!Line.isEmpty())
				stream << Line << "\n";

			for (int ChildIdx = 0; ChildIdx < Item->childCount(); ChildIdx++)
				WriteItem(Item->child(ChildIdx));
		};

		for (int ItemIdx = 0; ItemIdx < mOutputWidget->topLevelItemCount(); ItemIdx++)
			WriteItem(mOutputWidget->topLevelItem(ItemIdx));
	}

	QMessageBox::information(nullptr, QString("Save Log"), QString("The console log has been saved to %1").arg(log.fileName()));
}

void mlMainWindow::OnCleanXPaks()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	QString fileListString;
	QStringList fileList;
	QDirIterator it(Folder, QStringList() << "*.xpak", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		QString filepath = it.next();
		fileList.append(filepath);
		fileListString.append("\n" + QDir(Folder).relativeFilePath(filepath));
	}

	QString relativeFolder = QDir(mGamePath).relativeFilePath(Folder);

	if (fileList.count() == 0)
	{
		QMessageBox::information(this, QString("Clean XPaks (%1)").arg(relativeFolder), QString("There are no XPak's to clean!"));
		return;
	}

	for (int FileIdx = 0; FileIdx < fileList.count(); FileIdx++)
	{
		const QString file = fileList[FileIdx];
		qDebug() << file;
		QFile(file).remove();
	}
}

void mlMainWindow::OnDelete()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = GetItemContainerName(Item);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	if (!ConfirmDestructiveActionTwice(this, "Delete Folder", QString("folder '%1'").arg(QDir::toNativeSeparators(Folder)), "This deletes the folder and all of its contents."))
		return;

	QDir(Folder).removeRecursively();
	PopulateFileList();
}

void mlMainWindow::OnExport2BinChooseDirectory()
{
	const QString dir = QFileDialog::getExistingDirectory(mExport2BinGUIWidget, tr("Open Directory"), mToolsPath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	this->mExport2BinTargetDirWidget->setText(dir);

	QSettings Settings;
	Settings.setValue("Export2Bin_TargetDir", dir);
}

void mlMainWindow::OnExport2BinToggleOverwriteFiles()
{
	QSettings Settings;
	Settings.setValue("Export2Bin_OverwriteFiles", mExport2BinOverwriteWidget->isChecked());
}

void mlMainWindow::OnToggleFavorite()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	ToggleFavoriteEntry(GetItemFavoriteKey(ItemList[0]));
	PopulateFileList();
}

void mlMainWindow::BuildOutputReady(QString Output)
{
	if (Output.isEmpty())
		return;

	mPendingOutput += Output;
	mOutputFullText += Output;
	if (!mOutputFlushTimer.isActive())
		mOutputFlushTimer.start(16);
}

void mlMainWindow::FlushBuildOutput()
{
	QString Output = mPendingOutput;
	mPendingOutput.clear();
	if (Output.isEmpty())
		return;

	if (!mOutputWidget)
		return;

	QSettings Settings;
	if (!UseImprovedConsoleStyle(Settings))
	{
		if (!mOutputPlainWidget)
			return;

		QScrollBar* PlainScrollBar = mOutputPlainWidget->verticalScrollBar();
		const bool WasAtBottom = mOutputPlainAutoFollow;
		const int PreviousScrollValue = PlainScrollBar ? PlainScrollBar->value() : 0;
		QString OriginalOutput = Output;
		if (OriginalOutput.isEmpty())
			return;
		mOutputPlainWidget->moveCursor(QTextCursor::End);
		mOutputPlainWidget->insertPlainText(OriginalOutput);
		if (PlainScrollBar)
		{
			if (WasAtBottom)
				PlainScrollBar->setValue(PlainScrollBar->maximum());
			else
				PlainScrollBar->setValue(qMin(PreviousScrollValue, PlainScrollBar->maximum()));
		}
		return;
	}

	if (mOutputTabIndex != OutputLogTabFull)
	{
		RebuildOutputFromBuffer();
		return;
	}

	if (mOutputPlainWidget)
	{
		QStringList FilteredPlainLines;
		const QStringList PlainOutputLines = Output.replace("\r", "").split('\n');
		for (const QString& RawLine : PlainOutputLines)
		{
			const QString TrimmedLine = RawLine.trimmed();
			if (TrimmedLine.isEmpty())
				continue;
			if (!ShouldDisplayLogOutput(Settings, TrimmedLine, mOutputTabIndex))
				continue;
			FilteredPlainLines.append(RawLine);
		}

		if (!FilteredPlainLines.isEmpty())
		{
			QScrollBar* PlainScrollBar = mOutputPlainWidget->verticalScrollBar();
			const bool PlainWasAtBottom = mOutputPlainAutoFollow;
			const int PreviousPlainScrollValue = PlainScrollBar ? PlainScrollBar->value() : 0;
			mOutputPlainWidget->moveCursor(QTextCursor::End);
			mOutputPlainWidget->insertPlainText(FilteredPlainLines.join("\n") + "\n");
			if (PlainScrollBar)
			{
				if (PlainWasAtBottom)
					PlainScrollBar->setValue(PlainScrollBar->maximum());
				else
					PlainScrollBar->setValue(qMin(PreviousPlainScrollValue, PlainScrollBar->maximum()));
			}
		}
	}

	QScrollBar* ScrollBar = mOutputWidget->verticalScrollBar();
	const bool WasAtBottom = mOutputTreeAutoFollow;
	const int PreviousScrollValue = ScrollBar ? ScrollBar->value() : 0;
	const bool SliderWasActive = ScrollBar && ScrollBar->isSliderDown();
	QString NormalizedOutput = Output.replace("\r", "");
	if (NormalizedOutput.trimmed().isEmpty())
		return;

	QStringList OutputLines = NormalizedOutput.split('\n');
	const bool ImprovedConsole = true;
	mOutputWidget->setUpdatesEnabled(false);

	for (int LineIdx = 0; LineIdx < OutputLines.count(); LineIdx++)
	{
		const QString RawLine = OutputLines[LineIdx];
		const QString TrimmedLine = RawLine.trimmed();
		if (TrimmedLine.isEmpty())
			continue;

		if (!ShouldDisplayLogOutput(Settings, TrimmedLine, mOutputTabIndex))
			continue;

		const bool CurrentBlockIsUnrecoverable = mCurrentOutputBlockItem && IsUnrecoverableLogLine(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString());
		const bool ContinueUnrecoverableBlock = CurrentBlockIsUnrecoverable && IsUnrecoverableLogLine(TrimmedLine);
		const bool ContinueCurrentBlock = mCurrentOutputBlockItem && ShouldContinueCurrentLogBlock(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), TrimmedLine);
		const bool StartsNewBlock = !ContinueUnrecoverableBlock && !ContinueCurrentBlock && (!ImprovedConsole || !mCurrentOutputBlockItem || IsLogBlockHeader(TrimmedLine) || LogDetailIndentLevel(RawLine) <= 0);
		LogMessageKind MessageKind = DetectLogMessageKind(TrimmedLine);
		if (IsUnrecoverableLogLine(TrimmedLine))
			MessageKind = LogMessageError;
		const QColor OutputColor = ColorForLogMessageKind(Settings, MessageKind);

		if (StartsNewBlock)
		{
			if (mOutputWidget->topLevelItemCount() > 0)
			{
				QTreeWidgetItem* SpacerItem = new QTreeWidgetItem(mOutputWidget);
				SpacerItem->setData(0, ML_LOG_TEXT_ROLE, QString());
				SpacerItem->setData(0, ML_LOG_IS_HEADER_ROLE, false);
				SpacerItem->setFlags(SpacerItem->flags() & ~Qt::ItemIsSelectable);
				SpacerItem->setSizeHint(0, QSize(0, 10));
				QWidget* SpacerWidget = new QWidget(mOutputWidget);
				SpacerWidget->setStyleSheet("background: transparent;");
				mOutputWidget->setItemWidget(SpacerItem, 0, SpacerWidget);
			}

			mCurrentOutputBlockItem = new QTreeWidgetItem(mOutputWidget);
			mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, ImprovedConsole ? TrimmedLine : RawLine);
			mCurrentOutputBlockItem->setData(0, ML_LOG_IS_HEADER_ROLE, true);
			mCurrentOutputBlockItem->setData(0, ML_LOG_EXPANDED_ROLE, ImprovedConsole);
			mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), ImprovedConsole)));
			mCurrentOutputBlockItem->setExpanded(ImprovedConsole);

				const QColor BackgroundColor = ImprovedConsole
					? OutputBlockBackgroundColor(mThemeMode, MessageKind, mOutputBlockCounter++)
				: QColor(0, 0, 0, 0);
			mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 0, CreateLogBlockWidget(mOutputWidget, mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString(), OutputColor, BackgroundColor, ImprovedConsole));

			if (ImprovedConsole)
			{
				QWidget* ActionWidget = new QWidget(mOutputWidget);
				ActionWidget->setStyleSheet("background: transparent;");
				QHBoxLayout* ActionLayout = new QHBoxLayout(ActionWidget);
				ActionLayout->setContentsMargins(0, 0, 4, 0);
				ActionLayout->setSpacing(2);
				QToolButton* CopyButton = new QToolButton(ActionWidget);
				CopyButton->setText(QString::fromUtf8("\xE2\xA7\x89"));
				CopyButton->setToolTip("Copy block");
				CopyButton->setAutoRaise(true);
				CopyButton->setCursor(Qt::PointingHandCursor);
				CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #98a1aa; font-size: 12px; font-weight: 600;");
				ActionLayout->addWidget(CopyButton);
				QToolButton* ChevronButton = new QToolButton(ActionWidget);
				ChevronButton->setText(QString::fromUtf8("\xE2\x8C\x84"));
				ChevronButton->setToolTip("Collapse block");
				ChevronButton->setAutoRaise(true);
				ChevronButton->setCursor(Qt::PointingHandCursor);
				ChevronButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #c9d0d7; font-size: 11px; font-weight: 600;");
				QTreeWidgetItem* HeaderItem = mCurrentOutputBlockItem;
				connect(CopyButton, &QToolButton::clicked, this, [=]()
				{
					QApplication::clipboard()->setText(LogBlockText(HeaderItem));
					CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #6ee7a8; font-size: 12px; font-weight: 600;");
					QTimer::singleShot(1100, CopyButton, [CopyButton]()
					{
						if (CopyButton)
							CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #98a1aa; font-size: 12px; font-weight: 600;");
					});
				});
				connect(ChevronButton, &QToolButton::clicked, this, [=]()
				{
					QSettings Settings;
					const bool Expanded = HeaderItem->isExpanded();
					HeaderItem->setExpanded(!Expanded);
					HeaderItem->setData(0, ML_LOG_EXPANDED_ROLE, !Expanded);
					HeaderItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString(), !Expanded)));
					ChevronButton->setText(Expanded ? QString::fromUtf8("\xE2\x80\xBA") : QString::fromUtf8("\xE2\x8C\x84"));
					ChevronButton->setToolTip(Expanded ? "Expand block" : "Collapse block");
					LogMessageKind HeaderKind = DetectLogMessageKind(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString());
					if (IsUnrecoverableLogLine(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString()))
						HeaderKind = LogMessageError;
						UpdateLogBlockWidget(mOutputWidget->itemWidget(HeaderItem, 0), HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString(), ColorForLogMessageKind(Settings, HeaderKind), OutputBlockBackgroundColor(mThemeMode, HeaderKind, qMax(0, mOutputBlockCounter - 1)), !Expanded);
					mOutputWidget->doItemsLayout();
				});
				ActionLayout->addWidget(ChevronButton);
				mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 1, ActionWidget);
			}
			continue;
		}

		QString RenderedLine = RawLine;
		if (RenderedLine.trimmed() == TrimmedLine && LogDetailIndentLevel(RawLine) > 0)
			RenderedLine = QString(LogDetailIndentLevel(RawLine) * 4, ' ') + TrimmedLine;
		QString BlockText = mCurrentOutputBlockItem->data(0, ML_LOG_TEXT_ROLE).toString();
		if (!BlockText.isEmpty())
			BlockText += "\n";
		BlockText += ImprovedConsole ? RenderedLine : RawLine;
		mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, BlockText);
		mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(BlockText, mCurrentOutputBlockItem->data(0, ML_LOG_EXPANDED_ROLE).toBool())));
		LogMessageKind HeaderKind = DetectLogMessageKind(BlockText);
		if (IsUnrecoverableLogLine(BlockText))
			HeaderKind = LogMessageError;
			const QColor BackgroundColor = ImprovedConsole
				? OutputBlockBackgroundColor(mThemeMode, HeaderKind, qMax(0, mOutputBlockCounter - 1))
			: QColor(0, 0, 0, 0);
		UpdateLogBlockWidget(mOutputWidget->itemWidget(mCurrentOutputBlockItem, 0), BlockText, ColorForLogMessageKind(Settings, HeaderKind), BackgroundColor, mCurrentOutputBlockItem->data(0, ML_LOG_EXPANDED_ROLE).toBool());
	}
	mOutputWidget->setUpdatesEnabled(true);

	if (ScrollBar)
	{
		if (WasAtBottom && !SliderWasActive)
			ScrollBar->setValue(ScrollBar->maximum());
		else if (!SliderWasActive)
			ScrollBar->setValue(qMin(PreviousScrollValue, ScrollBar->maximum()));
	}
}

void mlMainWindow::BuildFinished()
{
	FlushBuildOutput();
	mlBuildThread* FinishedBuildThread = mBuildThread;
	mlConvertThread* FinishedConvertThread = mConvertThread;
	mBuildThread = NULL;
	mConvertThread = NULL;
	ResetBuildButtons();

	if (FinishedBuildThread)
		FinishedBuildThread->deleteLater();

	if (FinishedConvertThread)
		FinishedConvertThread->deleteLater();
}

void mlMainWindow::AppendOutputBlock(const QString& BlockText, const QSettings& Settings)
{
	if (!mOutputWidget || BlockText.trimmed().isEmpty())
		return;

	if (mOutputWidget->topLevelItemCount() > 0)
	{
		QTreeWidgetItem* SpacerItem = new QTreeWidgetItem(mOutputWidget);
		SpacerItem->setData(0, ML_LOG_TEXT_ROLE, QString());
		SpacerItem->setData(0, ML_LOG_IS_HEADER_ROLE, false);
		SpacerItem->setFlags(SpacerItem->flags() & ~Qt::ItemIsSelectable);
		SpacerItem->setSizeHint(0, QSize(0, 10));
		QWidget* SpacerWidget = new QWidget(mOutputWidget);
		SpacerWidget->setStyleSheet("background: transparent;");
		mOutputWidget->setItemWidget(SpacerItem, 0, SpacerWidget);
	}

	LogMessageKind HeaderKind = DetectLogMessageKind(BlockText);
	if (IsUnrecoverableLogLine(BlockText))
		HeaderKind = LogMessageError;
	const QColor OutputColor = ColorForLogMessageKind(Settings, HeaderKind);
	const QColor BackgroundColor = OutputBlockBackgroundColor(mThemeMode, HeaderKind, mOutputBlockCounter++);

	mCurrentOutputBlockItem = new QTreeWidgetItem(mOutputWidget);
	mCurrentOutputBlockItem->setData(0, ML_LOG_TEXT_ROLE, BlockText);
	mCurrentOutputBlockItem->setData(0, ML_LOG_IS_HEADER_ROLE, true);
	mCurrentOutputBlockItem->setData(0, ML_LOG_EXPANDED_ROLE, true);
	mCurrentOutputBlockItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(BlockText, true)));
	mCurrentOutputBlockItem->setExpanded(true);
	mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 0, CreateLogBlockWidget(mOutputWidget, BlockText, OutputColor, BackgroundColor, true));

	QWidget* ActionWidget = new QWidget(mOutputWidget);
	ActionWidget->setStyleSheet("background: transparent;");
	QHBoxLayout* ActionLayout = new QHBoxLayout(ActionWidget);
	ActionLayout->setContentsMargins(0, 0, 4, 0);
	ActionLayout->setSpacing(2);
	QToolButton* CopyButton = new QToolButton(ActionWidget);
	CopyButton->setText(QString::fromUtf8("\xE2\xA7\x89"));
	CopyButton->setToolTip("Copy block");
	CopyButton->setAutoRaise(true);
	CopyButton->setCursor(Qt::PointingHandCursor);
	CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #98a1aa; font-size: 12px; font-weight: 600;");
	ActionLayout->addWidget(CopyButton);
	QToolButton* ChevronButton = new QToolButton(ActionWidget);
	ChevronButton->setText(QString::fromUtf8("\xE2\x8C\x84"));
	ChevronButton->setToolTip("Collapse block");
	ChevronButton->setAutoRaise(true);
	ChevronButton->setCursor(Qt::PointingHandCursor);
	ChevronButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #c9d0d7; font-size: 11px; font-weight: 600;");
	QTreeWidgetItem* HeaderItem = mCurrentOutputBlockItem;
	connect(CopyButton, &QToolButton::clicked, this, [=]()
	{
		QApplication::clipboard()->setText(LogBlockText(HeaderItem));
		CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #6ee7a8; font-size: 12px; font-weight: 600;");
		QTimer::singleShot(1100, CopyButton, [CopyButton]()
		{
			if (CopyButton)
				CopyButton->setStyleSheet("background: transparent; border: 0; padding: 0 1px; color: #98a1aa; font-size: 12px; font-weight: 600;");
		});
	});
	connect(ChevronButton, &QToolButton::clicked, this, [=]()
	{
		QSettings ButtonSettings;
		const bool Expanded = HeaderItem->isExpanded();
		HeaderItem->setExpanded(!Expanded);
		HeaderItem->setData(0, ML_LOG_EXPANDED_ROLE, !Expanded);
		HeaderItem->setSizeHint(0, QSize(0, LogBlockWidgetHeight(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString(), !Expanded)));
		ChevronButton->setText(Expanded ? QString::fromUtf8("\xE2\x80\xBA") : QString::fromUtf8("\xE2\x8C\x84"));
		ChevronButton->setToolTip(Expanded ? "Expand block" : "Collapse block");
		LogMessageKind ButtonHeaderKind = DetectLogMessageKind(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString());
		if (IsUnrecoverableLogLine(HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString()))
			ButtonHeaderKind = LogMessageError;
			UpdateLogBlockWidget(mOutputWidget->itemWidget(HeaderItem, 0), HeaderItem->data(0, ML_LOG_TEXT_ROLE).toString(), ColorForLogMessageKind(ButtonSettings, ButtonHeaderKind), OutputBlockBackgroundColor(mThemeMode, ButtonHeaderKind, qMax(0, mOutputBlockCounter - 1)), !Expanded);
		mOutputWidget->doItemsLayout();
	});
	ActionLayout->addWidget(ChevronButton);
	mOutputWidget->setItemWidget(mCurrentOutputBlockItem, 1, ActionWidget);
}

void mlMainWindow::UpdateOutputConsoleMode()
{
	QSettings Settings;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool ShowPlainConsole = !ImprovedConsole || mOutputSelectionMode;
	if (mOutputTabs)
		mOutputTabs->setVisible(ImprovedConsole);
	if (mOutputWidget)
		mOutputWidget->setVisible(ImprovedConsole && !ShowPlainConsole);
	if (mOutputPlainWidget)
	{
		mOutputPlainWidget->setLineWrapMode(ImprovedConsole ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
		mOutputPlainWidget->setHorizontalScrollBarPolicy(ImprovedConsole ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		mOutputPlainWidget->setVisible(ShowPlainConsole);
	}
	if (mLogSelectionButton)
	{
		mLogSelectionButton->setVisible(ImprovedConsole);
		mLogSelectionButton->blockSignals(true);
		mLogSelectionButton->setChecked(mOutputSelectionMode);
		mLogSelectionButton->setText(mOutputSelectionMode ? "Block View" : "Text Select");
		mLogSelectionButton->blockSignals(false);
	}
}

void mlMainWindow::RebuildOutputFromBuffer()
{
	if (!mOutputWidget)
		return;

	QSettings Settings;
	const bool ImprovedConsole = UseImprovedConsoleStyle(Settings);
	const bool ShowPlainConsole = !ImprovedConsole || mOutputSelectionMode;
	const QString FullOutput = mOutputFullText;
	if (mOutputWidget)
		mOutputWidget->clear();
	if (mOutputPlainWidget)
		mOutputPlainWidget->clear();
	mCurrentOutputBlockItem = NULL;
	mOutputBlockCounter = 0;
	mOutputTreeAutoFollow = true;
	mOutputPlainAutoFollow = true;
	if (FullOutput.isEmpty())
		return;

	QStringList VisibleBlocks;
	for (const QString& BlockText : ExtractLogBlocks(FullOutput))
	{
		if (ShouldDisplayLogBlock(Settings, BlockText, mOutputTabIndex))
			VisibleBlocks.append(BlockText);
	}

	if (mOutputPlainWidget && ShowPlainConsole)
		mOutputPlainWidget->setPlainText(VisibleBlocks.join("\n\n"));

	if (!ImprovedConsole || ShowPlainConsole)
		return;

	mOutputWidget->setUpdatesEnabled(false);
	for (const QString& BlockText : VisibleBlocks)
		AppendOutputBlock(BlockText, Settings);
	mOutputWidget->setUpdatesEnabled(true);
}

Export2BinGroupBox::Export2BinGroupBox(QWidget* parent, mlMainWindow* parent_window) : QGroupBox(parent), parentWindow(parent_window)
{
	this->setAcceptDrops(true);
}

void Export2BinGroupBox::dragEnterEvent(QDragEnterEvent* event)
{
	event->acceptProposedAction();
}

void Export2BinGroupBox::dropEvent(QDropEvent* event)
{
	const QMimeData* mimeData = event->mimeData();

	if (parentWindow == NULL)
	{
		return;
	}

	if (mimeData->hasUrls())
	{
		QStringList pathList;
		QList<QUrl> urlList = mimeData->urls();

		QDir working_dir(parentWindow->mToolsPath);
		for (int i = 0; i < urlList.size(); i++)
		{
			pathList.append(urlList.at(i).toLocalFile());
		}
		
		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));

		bool allowOverwrite = this->parentWindow->mExport2BinOverwriteWidget->isChecked();

		QString outputDir = parentWindow->mExport2BinTargetDirWidget->text();
		parentWindow->StartConvertThread(pathList, outputDir, allowOverwrite);
		
		event->acceptProposedAction();
	}
}

void Export2BinGroupBox::dragLeaveEvent(QDragLeaveEvent* event)
{
	event->accept();
}
