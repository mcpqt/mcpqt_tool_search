// SPDX-License-Identifier: AGPL-3.0-only

#include "bm25.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mcp::bm25
{

  Index::Index(Options options)
      : m_options(options)
  {
    if(!std::isfinite(m_options.k1) || m_options.k1 < 0.0)
    {
      throw std::invalid_argument("BM25 k1 must be finite and non-negative");
    }
    if(!std::isfinite(m_options.b) || m_options.b < 0.0 || m_options.b > 1.0)
    {
      throw std::invalid_argument("BM25 b must be finite and in [0, 1]");
    }
  }

  void Index::addDocument(QString id, QStringList terms)
  {
    if(id.isEmpty())
    {
      throw std::invalid_argument("BM25 document id must not be empty");
    }

    if(auto existing = m_documents.find(id); existing != m_documents.end())
    {
      removeEntry(existing.value());
      m_documents.erase(existing);
    }

    Entry entry;
    entry.id = std::move(id);
    entry.terms = std::move(terms);
    for(const QString& term : entry.terms)
    {
      ++entry.frequencies[term];
    }
    addEntry(std::move(entry));
  }

  bool Index::removeDocument(const QString& id)
  {
    auto it = m_documents.find(id);
    if(it == m_documents.end())
    {
      return false;
    }
    removeEntry(it.value());
    m_documents.erase(it);
    return true;
  }

  void Index::clear(void)
  {
    m_documents.clear();
    m_document_frequency.clear();
    m_total_length = 0;
  }

  qsizetype Index::documentCount(void) const noexcept
  {
    return m_documents.size();
  }

  double Index::averageDocumentLength(void) const noexcept
  {
    if(m_documents.isEmpty())
    {
      return 0.0;
    }
    return static_cast<double>(m_total_length)
           / static_cast<double>(m_documents.size());
  }

  bool Index::contains(const QString& id) const
  {
    return m_documents.contains(id);
  }

  double Index::score(const QString& id, const QStringList& query) const
  {
    const auto document = m_documents.constFind(id);
    if(document == m_documents.constEnd() || query.isEmpty())
    {
      return 0.0;
    }

    const double document_count = static_cast<double>(m_documents.size());
    const double average_length = averageDocumentLength();
    if(average_length <= 0.0)
    {
      return 0.0;
    }

    const Entry& entry = document.value();
    const double document_length = static_cast<double>(entry.terms.size());
    const double length_norm
        = 1.0 - m_options.b
          + m_options.b * (document_length / average_length);

    double total = 0.0;
    for(const QString& term : query)
    {
      const auto frequency_it = entry.frequencies.constFind(term);
      if(frequency_it == entry.frequencies.constEnd())
      {
        continue;
      }
      const double frequency = static_cast<double>(frequency_it.value());
      if(frequency <= 0.0)
      {
        continue;
      }

      const auto document_frequency_it = m_document_frequency.constFind(term);
      const double document_frequency
          = document_frequency_it == m_document_frequency.constEnd()
                ? 0.0
                : static_cast<double>(document_frequency_it.value());

      const double idf = std::log(
          1.0
          + (document_count - document_frequency + 0.5)
                / (document_frequency + 0.5));
      const double numerator = frequency * (m_options.k1 + 1.0);
      const double denominator = frequency + m_options.k1 * length_norm;
      total += idf * numerator / denominator;
    }
    return total;
  }

  QList<Hit> Index::search(const QStringList& query, qsizetype limit) const
  {
    QList<Hit> hits;
    if(query.isEmpty() || m_documents.isEmpty() || limit == 0)
    {
      return hits;
    }

    hits.reserve(m_documents.size());
    for(const Entry& entry : m_documents)
    {
      const double value = score(entry.id, query);
      if(value > 0.0)
      {
        hits.append(Hit{ entry.id, value });
      }
    }

    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
      if(a.score != b.score)
      {
        return a.score > b.score;
      }
      return a.id < b.id;
    });

    if(limit > 0 && hits.size() > limit)
    {
      hits.resize(limit);
    }
    return hits;
  }

  void Index::removeEntry(const Entry& entry)
  {
    m_total_length -= entry.terms.size();
    for(auto it = entry.frequencies.cbegin(); it != entry.frequencies.cend();
        ++it)
    {
      auto document_frequency = m_document_frequency.find(it.key());
      if(document_frequency == m_document_frequency.end())
      {
        continue;
      }
      if(--document_frequency.value() <= 0)
      {
        m_document_frequency.erase(document_frequency);
      }
    }
  }

  void Index::addEntry(Entry entry)
  {
    m_total_length += entry.terms.size();
    for(auto it = entry.frequencies.cbegin(); it != entry.frequencies.cend();
        ++it)
    {
      ++m_document_frequency[it.key()];
    }
    const QString key = entry.id;
    m_documents.insert(key, std::move(entry));
  }

} // namespace mcp::bm25
