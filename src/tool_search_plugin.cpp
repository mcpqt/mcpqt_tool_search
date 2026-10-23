// SPDX-License-Identifier: AGPL-3.0-only
//
// Tool search plugin. Reimplements the OpenCode tool_search and
// tool_search_regex tools as an mcpqt_host plugin. The catalog is read from the
// host's toolList/toolDetails services so the plugin can search every tool
// registered on the active MCP server.

#include "plugin.h"

#include "bm25.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <algorithm>
#include <stdexcept>

namespace
{

  struct CatalogEntry
  {
    QString id;
    QString name;
    QString description;
    QString source;
    QStringList terms;
  };

  struct RankedEntry
  {
    CatalogEntry entry;
    double score = 0.0;
  };

  QStringList tokenize(const QString& text)
  {
    static const QRegularExpression non_alnum(
        QStringLiteral("[^a-z0-9\\s]"));
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    QString normalized = text.toLower();
    normalized.replace(non_alnum, QStringLiteral(" "));
    return normalized.split(whitespace, Qt::SkipEmptyParts);
  }

  QList<CatalogEntry> loadCatalog(const mcp::HostInterface& host)
  {
    QStringList names;
    try
    {
      const QVariant raw = host.callFunc(
          QStringLiteral("QVariant mcp::Host::toolList()"), {});
      names = raw.toStringList();
    }
    catch(...)
    {
      // Fallback for hosts without the tool catalog service.
      names = host.getFuncs ? host.getFuncs() : QStringList{};
    }

    names.sort();
    QList<CatalogEntry> entries;
    entries.reserve(names.size());
    for(const QString& name : names)
    {
      CatalogEntry entry;
      entry.id = name;
      entry.name = name;
      entry.source = QStringLiteral("plugin");

      if(host.callFunc)
      {
        try
        {
          const QVariant raw = host.callFunc(
              QStringLiteral("QVariant mcp::Host::toolDetails(QString)"),
              { QVariant(name) });
          const QVariantMap details = raw.toMap();
          if(!details.isEmpty())
          {
            entry.name
                = details.value(QStringLiteral("name"), name).toString();
            entry.description
                = details.value(QStringLiteral("description")).toString();
            entry.source = details.value(QStringLiteral("source"),
                                         entry.source)
                              .toString();
          }
        }
        catch(...)
        {
          // Keep the fallback entry when a detail lookup fails.
        }
      }

      entry.terms
          = tokenize(entry.name + QLatin1Char(' ') + entry.description);
      entries.append(entry);
    }
    return entries;
  }

  bool sourceMatches(const CatalogEntry& entry, const QString& category)
  {
    return category.isEmpty() || category == QStringLiteral("all")
           || entry.source == category;
  }

  QList<RankedEntry> searchBm25(const QList<CatalogEntry>& entries,
                                const QString& query,
                                const QString& category,
                                qsizetype limit)
  {
    mcp::bm25::Index index(mcp::bm25::Options{ .k1 = 0.9, .b = 0.4 });
    for(const CatalogEntry& entry : entries)
    {
      index.addDocument(entry.id, entry.terms);
    }

    QHash<QString, CatalogEntry> by_id;
    for(const CatalogEntry& entry : entries)
    {
      by_id.insert(entry.id, entry);
    }

    QList<RankedEntry> out;
    for(const mcp::bm25::Hit& hit : index.search(tokenize(query), limit))
    {
      const CatalogEntry entry = by_id.value(hit.id);
      if(sourceMatches(entry, category))
      {
        out.append(RankedEntry{ entry, hit.score });
      }
    }
    return out;
  }

  QList<RankedEntry> runRegexSearch(const QList<CatalogEntry>& entries,
                                    const QString& pattern,
                                    const QString& category,
                                    qsizetype limit)
  {
    const QRegularExpression regex(
        pattern, QRegularExpression::CaseInsensitiveOption);
    if(!regex.isValid())
    {
      throw std::invalid_argument(regex.errorString().toStdString());
    }

    QList<RankedEntry> out;
    for(const CatalogEntry& entry : entries)
    {
      if(!sourceMatches(entry, category))
      {
        continue;
      }
      if(regex.match(entry.name).hasMatch()
         || regex.match(entry.description).hasMatch())
      {
        out.append(RankedEntry{ entry, 0.0 });
        if(limit > 0 && out.size() >= limit)
        {
          break;
        }
      }
    }
    return out;
  }

  QVariant makeResult(const QString& key,
                      const QString& query,
                      const QList<RankedEntry>& hits)
  {
    QJsonArray tools;
    QStringList names;
    for(const RankedEntry& hit : hits)
    {
      tools.append(hit.entry.id);
      names.append(hit.entry.id);
    }

    QJsonObject metadata;
    metadata.insert(key, query);
    metadata.insert(QStringLiteral("count"), static_cast<int>(hits.size()));
    metadata.insert(QStringLiteral("tools"), tools);
    metadata.insert(QStringLiteral("displayOutput"),
                    names.join(QLatin1Char('\n')));

    QJsonObject result;
    if(hits.isEmpty())
    {
      result.insert(QStringLiteral("title"),
                    QStringLiteral("No tools found"));
      result.insert(QStringLiteral("metadata"), metadata);
      result.insert(QStringLiteral("output"),
                    QStringLiteral("No tools found matching \"%1\". Try a "
                                   "different search term.")
                        .arg(query));
      return QVariant::fromValue(result);
    }

    result.insert(QStringLiteral("title"),
                  QStringLiteral("Found %1 tools").arg(hits.size()));
    result.insert(QStringLiteral("metadata"), metadata);
    result.insert(QStringLiteral("output"),
                  QStringLiteral(
                      "Now you can also use the following tools: %1")
                      .arg(names.join(QStringLiteral(", "))));
    return QVariant::fromValue(result);
  }

  QVariant invalidRegexResult(const QString& pattern, const QString& reason)
  {
    QJsonObject metadata;
    metadata.insert(QStringLiteral("pattern"), pattern);
    metadata.insert(QStringLiteral("count"), 0);
    metadata.insert(QStringLiteral("tools"), QJsonArray{});
    metadata.insert(QStringLiteral("displayOutput"),
                    QStringLiteral("Invalid regex pattern: %1").arg(pattern));

    QJsonObject result;
    result.insert(QStringLiteral("title"), QStringLiteral("Invalid regex"));
    result.insert(QStringLiteral("metadata"), metadata);
    result.insert(QStringLiteral("output"),
                  QStringLiteral("Invalid regex pattern: \"%1\". %2")
                      .arg(pattern, reason));
    return QVariant::fromValue(result);
  }

  class ToolSearchPlugin : public mcp::PluginInterface
  {
    Q_OBJECT
    Q_CLASSINFO("id", "tool-search")
    Q_CLASSINFO("version", "0.1.0")
    Q_CLASSINFO("description",
                "Search available tools by name or description.")
    Q_CLASSINFO("license", "AGPL-3.0-only")
    Q_PLUGIN_METADATA(IID MCP_PLUGIN_IID FILE "tool_search_plugin.json")
    Q_INTERFACES(mcp::PluginInterface)

  public:
    explicit ToolSearchPlugin(QObject* parent = nullptr)
        : mcp::PluginInterface(parent)
    {
    }

    void initialize(const mcp::HostInterface& host) override
    {
      m_host = host;
      registerPublicSlots();
    }

    void shutdown(void) override {}

  public slots:
    QVariant search(QString query, QString category)
    {
      const QList<CatalogEntry> entries = loadCatalog(m_host);
      const QList<RankedEntry> hits
          = searchBm25(entries, query, category, 5);
      return makeResult(QStringLiteral("query"), query, hits);
    }

    QVariant searchRegex(QString pattern, QString category)
    {
      const QList<CatalogEntry> entries = loadCatalog(m_host);
      try
      {
        const QList<RankedEntry> hits
            = runRegexSearch(entries, pattern, category, 10);
        return makeResult(QStringLiteral("pattern"), pattern, hits);
      }
      catch(const std::exception& e)
      {
        return invalidRegexResult(pattern, QString::fromUtf8(e.what()));
      }
    }
  };

} // namespace

#include "tool_search_plugin.moc"
