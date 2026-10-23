// SPDX-License-Identifier: AGPL-3.0-only
//
// Okapi BM25 ranking index.
//
// Scores documents against a query using the standard BM25 term-weighting
// formula. Callers supply pre-tokenized terms; this component deliberately
// does not impose a tokenizer so the same index can serve different corpora
// (tool names, descriptions, source text) without hidden normalization.
//
// Reference formula:
//   score(D, Q) = sum_i IDF(q_i)
//                 * f(q_i, D) * (k1 + 1)
//                 / (f(q_i, D) + k1 * (1 - b + b * |D| / avgdl))
//
//   IDF(q_i) = ln(1 + (N - n(q_i) + 0.5) / (n(q_i) + 0.5))

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <QtGlobal>

namespace mcp::bm25
{

  /// Tuning parameters for the ranking function.
  struct Options
  {
    /// Term-frequency saturation. Typical range: [1.2, 2.0].
    double k1 = 1.2;

    /// Document-length normalization. 0 disables it; typical value: 0.75.
    double b = 0.75;
  };

  /// One ranked result.
  struct Hit
  {
    QString id;
    double score = 0.0;
  };

  /// In-memory BM25 index over pre-tokenized documents.
  ///
  /// Document ids must be unique. Adding a document with an existing id
  /// replaces it and updates collection statistics accordingly. Empty term
  /// lists are allowed; their document length is zero.
  class Index
  {
  public:
    explicit Index(Options options = {});

    /// Add or replace a document. `terms` should already be normalized by the
    /// caller (for example, lower-cased and split on word boundaries).
    void addDocument(QString id, QStringList terms);

    /// Remove a document by id. Returns false when the id is unknown.
    bool removeDocument(const QString& id);

    /// Drop all documents and collection statistics.
    void clear(void);

    [[nodiscard]] qsizetype documentCount(void) const noexcept;
    [[nodiscard]] double averageDocumentLength(void) const noexcept;
    [[nodiscard]] bool contains(const QString& id) const;

    /// BM25 score of one document for `query`. Returns 0 when the document or
    /// query is empty, or when no query term occurs in the document.
    [[nodiscard]] double score(const QString& id,
                               const QStringList& query) const;

    /// Score every document and return hits sorted by descending score, then
    /// ascending id for deterministic ties. Documents scoring zero are
    /// omitted. A negative `limit` returns all hits; 0 returns none.
    [[nodiscard]] QList<Hit> search(const QStringList& query,
                                    qsizetype limit = -1) const;

  private:
    struct Entry
    {
      QString id;
      QStringList terms;
      QHash<QString, qsizetype> frequencies;
    };

    void removeEntry(const Entry& entry);
    void addEntry(Entry entry);

    Options m_options;
    QHash<QString, Entry> m_documents;
    QHash<QString, qsizetype> m_document_frequency;
    qsizetype m_total_length = 0;
  };

} // namespace mcp::bm25
