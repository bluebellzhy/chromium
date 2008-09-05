// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/history/query_parser.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/word_iterator.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/scoped_vector.h"
#include "unicode/uscript.h"

// For CJK ideographs and Korean Hangul, even a single character
// can be useful in prefix matching, but that may give us too many
// false positives. Moreover, the current ICU word breaker gives us
// back every single Chinese character as a word so that there's no
// point doing anything for them and we only adjust the minimum length
// to 2 for Korean Hangul while using 3 for others. This is a temporary
// hack until we have a segmentation support.
static inline bool IsWordLongEnoughForPrefixSearch(const std::wstring& word)
{
  DCHECK(word.size() > 0);
  size_t minimum_length = 3;
  // We intentionally exclude Hangul Jamos (both Conjoining and compatibility)
  // because they 'behave like' Latin letters. Moreover, we should
  // normalize the former before reaching here.
  if (0xAC00 <= word[0] && word[0] <= 0xD7A3)
    minimum_length = 2;
  return word.size() >= minimum_length;
}

// Inheritance structure:
// Queries are represented as trees of QueryNodes.
// QueryNodes are either a collection of subnodes (a QueryNodeList)
// or a single word (a QueryNodeWord).

// A QueryNodeWord is a single word in the query.
class QueryNodeWord : public QueryNode {
 public:
  QueryNodeWord(const std::wstring& word) : word_(word), literal_(false) {}
  virtual ~QueryNodeWord() {}
  virtual int AppendToSQLiteQuery(std::wstring* query) const;
  virtual bool IsWord() const { return true; }

  const std::wstring& word() const { return word_; }
  void set_literal(bool literal) { literal_ = literal; }

  virtual bool HasMatchIn(const std::vector<std::wstring>& words) const;

  virtual bool Matches(const std::wstring& word, bool exact) const;

 private:
  std::wstring word_;
  bool literal_;
};

bool QueryNodeWord::HasMatchIn(const std::vector<std::wstring>& words) const {
  for (size_t i = 0; i < words.size(); ++i) {
    if (Matches(words[i], false))
      return true;
  }
  return false;
}

bool QueryNodeWord::Matches(const std::wstring& word, bool exact) const {
  if (exact || !IsWordLongEnoughForPrefixSearch(word_))
    return word == word_;
  return word.size() >= word_.size() &&
         (word_.compare(0, word_.size(), word, 0, word_.size()) == 0);
}

int QueryNodeWord::AppendToSQLiteQuery(std::wstring* query) const {
  query->append(word_);

  // Use prefix search if we're not literal and long enough.
  if (!literal_ && IsWordLongEnoughForPrefixSearch(word_))
    *query += L'*';
  return 1;
}

// A QueryNodeList has a collection of child QueryNodes
// which it cleans up after.
class QueryNodeList : public QueryNode {
 public:
  virtual ~QueryNodeList();

  virtual int AppendToSQLiteQuery(std::wstring* query) const {
    return AppendChildrenToString(query);
  }
  virtual bool IsWord() const { return false; }

  void AddChild(QueryNode* node) { children_.push_back(node); }

  typedef std::vector<QueryNode*> QueryNodeVector;
  QueryNodeVector* children() { return &children_; }

  // Remove empty subnodes left over from other parsing.
  void RemoveEmptySubnodes();

  // QueryNodeList is never used with Matches or HasMatchIn.
  virtual bool Matches(const std::wstring& word, bool exact) const {
    NOTREACHED();
    return false;
  }
  virtual bool HasMatchIn(const std::vector<std::wstring>& words) const {
    NOTREACHED();
    return false;
  }

 protected:
  int AppendChildrenToString(std::wstring* query) const;

  QueryNodeVector children_;
};

QueryNodeList::~QueryNodeList() {
  for (QueryNodeVector::iterator node = children_.begin();
       node != children_.end(); ++node)
    delete *node;
}

int QueryNodeList::AppendChildrenToString(std::wstring* query) const {
  int num_words = 0;
  for (QueryNodeVector::const_iterator node = children_.begin();
       node != children_.end(); ++node) {
    if (node != children_.begin())
      query->push_back(L' ');
    num_words += (*node)->AppendToSQLiteQuery(query);
  }
  return num_words;
}

// A QueryNodePhrase is a phrase query ("quoted").
class QueryNodePhrase : public QueryNodeList {
 public:
  virtual int AppendToSQLiteQuery(std::wstring* query) const {
    query->push_back(L'"');
    int num_words = AppendChildrenToString(query);
    query->push_back(L'"');
    return num_words;
  }

  virtual bool Matches(const std::wstring& word, bool exact) const;
  virtual bool HasMatchIn(const std::vector<std::wstring>& words) const;
};

bool QueryNodePhrase::Matches(const std::wstring& word, bool exact) const {
  NOTREACHED();
  return false;
}

bool QueryNodePhrase::HasMatchIn(const std::vector<std::wstring>& words) const {
  if (words.size() < children_.size())
    return false;

  for (size_t i = 0, max = words.size() - children_.size() + 1; i < max; ++i) {
    bool matched_all = true;
    for (size_t j = 0; j < children_.size(); ++j) {
      if (!children_[j]->Matches(words[i + j], true)) {
        matched_all = false;
        break;
      }
    }
    if (matched_all)
      return true;
  }
  return false;
}

QueryParser::QueryParser() {
}

// Returns true if the character is considered a quote.
static bool IsQueryQuote(wchar_t ch) {
  return ch == '"' ||
         ch == 0xab ||    // left pointing double angle bracket
         ch == 0xbb ||    // right pointing double angle bracket
         ch == 0x201c ||  // left double quotation mark
         ch == 0x201d ||  // right double quotation mark
         ch == 0x201e;    // double low-9 quotation mark
}

int QueryParser::ParseQuery(const std::wstring& query,
                            std::wstring* sqlite_query) {
  QueryNodeList root;
  if (!ParseQueryImpl(query, &root))
    return 0;
  return root.AppendToSQLiteQuery(sqlite_query);
}

void QueryParser::ParseQuery(const std::wstring& query,
                             std::vector<QueryNode*>* nodes) {
  QueryNodeList root;
  if (ParseQueryImpl(l10n_util::ToLower(query), &root))
    nodes->swap(*root.children());
}

bool QueryParser::DoesQueryMatch(const std::wstring& text,
                                 const std::vector<QueryNode*>& query_nodes) {
  if (query_nodes.empty())
    return false;

  std::vector<std::wstring> query_words;
  ExtractWords(l10n_util::ToLower(text), &query_words);

  if (query_words.empty())
    return false;

  for (size_t i = 0; i < query_nodes.size(); ++i) {
    if (!query_nodes[i]->HasMatchIn(query_words))
      return false;
  }
  return true;
}

bool QueryParser::ParseQueryImpl(const std::wstring& query,
                                QueryNodeList* root) {
  WordIterator iter(query, WordIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return false;

  // To handle nesting, we maintain a stack of QueryNodeLists.
  // The last element (back) of the stack contains the current, deepest node.
  std::vector<QueryNodeList*> query_stack;
  query_stack.push_back(root);

  bool in_quotes = false;  // whether we're currently in a quoted phrase
  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::wstring word = iter.GetWord();

      QueryNodeWord* word_node = new QueryNodeWord(word);
      if (in_quotes)
        word_node->set_literal(true);
      query_stack.back()->AddChild(word_node);
    } else {  // Punctuation.
      if (IsQueryQuote(query[iter.prev()])) {
        if (!in_quotes) {
          QueryNodeList* quotes_node = new QueryNodePhrase;
          query_stack.back()->AddChild(quotes_node);
          query_stack.push_back(quotes_node);
          in_quotes = true;
        } else {
          query_stack.pop_back();  // stop adding to the quoted phrase
          in_quotes = false;
        }
      }
    }
  }

  root->RemoveEmptySubnodes();
  return true;
}

void QueryParser::ExtractWords(const std::wstring& text,
                               std::vector<std::wstring>* words) {
  WordIterator iter(text, WordIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::wstring word = iter.GetWord();
      if (!word.empty())
        words->push_back(word);
    }
  }
}

void QueryNodeList::RemoveEmptySubnodes() {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->IsWord())
      continue;

    QueryNodeList* list_node = static_cast<QueryNodeList*>(children_[i]);
    list_node->RemoveEmptySubnodes();
    if (list_node->children()->empty()) {
      children_.erase(children_.begin() + i);
      --i;
      delete list_node;
    }
  }
}
