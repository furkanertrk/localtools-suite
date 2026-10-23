#ifndef LOCALTOOLS_CORE_TOOL_REGISTRY_H
#define LOCALTOOLS_CORE_TOOL_REGISTRY_H

#include <QString>
#include <QStringList>
#include <QIcon>
#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include "../ui/common/icon_utils.h"
#include "tool_operation.h"

namespace LocalTools::Core {

/**
 * Metadata descriptor for a single tool.
 * Used by Home page, Sidebar, Search, and Favorites systems.
 */
struct ToolDescriptor {
    QString id;                     // Unique ID: "pdf.merge", "image.convert.jpg-to-png"
    QString name;                   // Display name: "Merge PDF"
    QString description;            // Short description for card/tooltip
    QString category;               // Top-level: "PDF", "Images", "Video", "Documents", "Files"
    QString subcategory;            // Sub-group: "Organize", "Convert", "Compress"
    LocalTools::UI::IconType iconType; // Icon enum for display
    QStringList keywords;           // Search keywords (Turkish + English)
    QStringList inputExtensions;    // Accepted input extensions: {".pdf", ".jpg", ".png"}
    QStringList outputExtensions;   // Generated output extensions
    bool supportsBatch{false};      // Can process multiple files
    bool isAvailable{true};         // false = "coming soon" badge
    int sortOrder{0};               // Display order within category
    int legacyToolId{-1};           // V1 toolId for backward compatibility with HomePage signals

    // Factory function to create an IToolOperation for this tool.
    // nullptr if tool is not yet implemented (isAvailable=false).
    using OperationFactory = std::function<std::unique_ptr<IToolOperation>()>;
    OperationFactory createOperation;
};

/**
 * Central registry of all available tools.
 * Provides search, filter, and lookup capabilities.
 * Tools register themselves at startup.
 */
class ToolRegistry {
public:
    static ToolRegistry& instance();

    // Registration & Localization
    void registerTool(ToolDescriptor descriptor);
    void retranslateTools();

    // Queries
    const std::vector<ToolDescriptor>& allTools() const { return m_tools; }
    std::vector<const ToolDescriptor*> toolsByCategory(const QString& category) const;
    std::vector<const ToolDescriptor*> toolsBySubcategory(const QString& category, const QString& subcategory) const;
    const ToolDescriptor* toolById(const QString& id) const;
    const ToolDescriptor* toolByLegacyId(int legacyToolId) const;

    // Search: returns tools matching query against name, description, keywords, extensions
    std::vector<const ToolDescriptor*> search(const QString& query) const;

    // Categories: returns unique category names in display order
    QStringList categories() const;

    // Subcategories for a given category
    QStringList subcategories(const QString& category) const;

    // Favorites
    void setFavorite(const QString& toolId, bool isFavorite);
    bool isFavorite(const QString& toolId) const;
    std::vector<const ToolDescriptor*> favorites() const;

private:
    ToolRegistry();
    ~ToolRegistry() = default;

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    void registerPdfTools();
    void registerImageTools();
    void registerDocumentTools();
    void registerFileTools();
    void registerVideoTools();
    void registerAudioTools();
    void registerQuickTools();

    std::vector<ToolDescriptor> m_tools;
    QStringList m_favoriteIds; // Persisted via SettingsService
};

} // namespace LocalTools::Core

#endif // LOCALTOOLS_CORE_TOOL_REGISTRY_H
