//---------------------------------------------------------------------------
// CreateDatabaseThread.cpp
//
// A class for creating a database in the background.
//
// Copyright 2006-2010 Michael W Thelen <mthelen@gmail.com>.
//
// This file is part of Zyzzyva.
//
// Zyzzyva is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Zyzzyva is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//---------------------------------------------------------------------------

#include "CreateDatabaseThread.h"
#include "LetterBag.h"
#include "MainSettings.h"
#include "WordEngine.h"
#include "Auxil.h"
#include "Defs.h"
#include <QtSql>

const int MAX_DEFINITION_LINKS = 3;
const int PROGRESS_STEP = 1000;
const QString DB_CONNECTION_NAME = "CreateDatabaseThread";

using namespace Defs;

//---------------------------------------------------------------------------
//  run
//
//! Create the database.
//---------------------------------------------------------------------------
void
CreateDatabaseThread::run()
{
    runPrivate();
}

//---------------------------------------------------------------------------
//  runPrivate
//
//! Create the database.
//---------------------------------------------------------------------------
void
CreateDatabaseThread::runPrivate()
{
    int numSteps = 0;

    {
        // Create empty database
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE",
                                                    DB_CONNECTION_NAME);
        db.setDatabaseName(dbFilename);
        if (!db.open())
            return;

        // Start at 1% progress
        emit(steps(100));
        emit(progress(1));

        // Total number of progress steps is number of words times the number
        // of lines that increment stepNum in all the code that is called
        // below.
        int stepNumIncs = 8;
        int numWords = wordEngine->getNumWords(lexiconName);
        int baseProgress = numWords * stepNumIncs / 99;
        numSteps = numWords * stepNumIncs + baseProgress + 1;
        emit steps(numSteps);
        int stepNum = baseProgress;
        emit progress(stepNum);

        createTables(db);
        createIndexes(db);
        // insertWords increments stepNum 3 times
        insertWords(db, stepNum);
        // updateProbabilityOrder increments stepNum 3 times for each word
        // because of 0, 1, 2 blanks
        updateProbabilityOrder(db, stepNum);
        updateDefinitions(db, stepNum);
        updateDefinitionLinks(db, stepNum);
    }

    cleanup();
    emit progress(numSteps);
}

//---------------------------------------------------------------------------
//  createTables
//
//! Create the database tables.
//
//! @param db the database
//---------------------------------------------------------------------------
void
CreateDatabaseThread::createTables(QSqlDatabase& db)
{
    QSqlQuery query (db);

    query.exec("CREATE TABLE words (word varchar(16), length integer, "
        "alphagram varchar(16), num_anagrams integer, "
        "num_unique_letters integer, num_vowels integer, "
        "point_value integer, front_hooks varchar(32), "
        "back_hooks varchar(32), is_front_hook integer, "
        "is_back_hook integer, lexicon_symbols varchar(16), "
        "definition varchar(256))");

    query.exec("CREATE TABLE probability (word varchar(16), "
        "length integer, num_blanks integer, "
        "combinations integer, probability_order integer, "
        "min_probability_order integer, max_probability_order integer)");

    query.exec("CREATE TABLE db_version (version integer)");
    query.exec("INSERT into db_version (version) VALUES (" +
               QString::number(CURRENT_DATABASE_VERSION) + ")");

    query.exec("CREATE TABLE lexicon_date (date date)");
    query.prepare("INSERT into lexicon_date (date) VALUES (?)");
    query.bindValue(0, Auxil::lexiconToDate(lexiconName));
    query.exec();

    query.exec("CREATE TABLE lexicon_file (file varchar(256))");
    query.prepare("INSERT into lexicon_file (file) VALUES (?)");
    query.bindValue(0, wordEngine->getLexiconFile(lexiconName));
    query.exec();
}

//---------------------------------------------------------------------------
//  createIndexes
//
//! Create the database indexes.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::createIndexes(QSqlDatabase& db)
{
    QSqlQuery query (db);

    // FIXME: more indexes on words table would help?

    // Indexes on words table
    query.exec("CREATE UNIQUE INDEX word_index on words (word)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX word_length_index on words (length)");
    if (cancelled)
        return;

    // Indexes on probability table
    query.exec("CREATE INDEX prob_word_blanks_index on probability "
               "(word, num_blanks)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX prob_word_index on probability (word)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX prob_length_index on probability (length)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX prob_blanks_index on probability (num_blanks)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX prob_order on probability "
               "(num_blanks, probability_order)");
    if (cancelled)
        return;

    query.exec("CREATE INDEX prob_min_max_order on probability "
               "(num_blanks, min_probability_order, max_probability_order)");
    if (cancelled)
        return;
}

//---------------------------------------------------------------------------
//  insertWords
//
//! Insert words into the database.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::insertWords(QSqlDatabase& db, int& stepNum)
{
    LetterBag letterBag;
    QStringList letters;
    letters << "A" << "B" << "C" << "D" << "E" << "F" << "G" << "H" <<
        "I" << "J" << "K" << "L" << "M" << "N" << "O" << "P" << "Q" <<
        "R" << "S" << "T" << "U" << "V" << "W" << "X" << "Y" << "Z";

    SearchCondition searchCondition;
    searchCondition.type = SearchCondition::Length;
    SearchSpec searchSpec;
    searchSpec.conditions.append(searchCondition);

    QList<LexiconStyle> lexStyles = MainSettings::getWordListLexiconStyles();
    QMutableListIterator<LexiconStyle> it (lexStyles);
    while (it.hasNext()) {
        const LexiconStyle& style = it.next();
        if ((style.lexicon != lexiconName) ||
            !wordEngine->lexiconIsLoaded(style.compareLexicon))
        {
            it.remove();
        }
    }

    QSqlQuery transactionQuery ("BEGIN TRANSACTION", db);
    QSqlQuery query (db);

    QMap<QString, int> numAnagramsMap;

    for (int length = 1; length <= MAX_WORD_LEN; ++length) {
        searchSpec.conditions[0].minValue = length;
        searchSpec.conditions[0].maxValue = length;

        // Do a word graph search because we're still building the database!
        QStringList words = wordEngine->wordGraphSearch(lexiconName,
                                                        searchSpec);

        query.prepare("INSERT INTO words (word, length, "
                      "alphagram, num_unique_letters, num_vowels, "
                      "point_value, front_hooks, back_hooks, "
                      "is_front_hook, is_back_hook, lexicon_symbols) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        // Insert words with length, hooks
        foreach (const QString& word, words) {
            int numUniqueLetters = Auxil::getNumUniqueLetters(word);
            int numVowels = Auxil::getNumVowels(word);

            int pointValue = 0;
            for (int i = 0; i < word.length(); ++i) {
                pointValue += letterBag.getLetterValue(word.at(i));
            }

            QString alphagram = Auxil::getAlphagram(word);
            if (numAnagramsMap.contains(alphagram))
                ++numAnagramsMap[alphagram];
            else
                numAnagramsMap[alphagram] = 1;

            int isFrontHook = wordEngine->isAcceptable(
                lexiconName, word.right(word.length() - 1)) ? 1 : 0;
            int isBackHook = wordEngine->isAcceptable(
                lexiconName, word.left(word.length() - 1)) ? 1 : 0;

            QString front, back;
            foreach (const QString& letter, letters) {
                if (wordEngine->isAcceptable(lexiconName, letter + word))
                    front += letter;
                if (wordEngine->isAcceptable(lexiconName, word + letter))
                    back += letter;
            }

            // Populate words and hooks with symbols
            QString symbolStr;
            if (!lexStyles.isEmpty()) {
                QListIterator<LexiconStyle> it (lexStyles);
                while (it.hasNext()) {
                    const LexiconStyle& style = it.next();
                    bool acceptable =
                        wordEngine->isAcceptable(style.compareLexicon, word);
                    if (!(acceptable ^ style.inCompareLexicon))
                        symbolStr += style.symbol;
                }

                // Populate front hooks with symbols
                for (int i = 0; i < front.length(); ++i) {
                    QChar c = front[i];
                    QString hookWord = c.toUpper() + word;

                    it.toFront();
                    while (it.hasNext()) {
                        const LexiconStyle& style = it.next();
                        bool acceptable = wordEngine->isAcceptable(
                            style.compareLexicon, hookWord);

                        if (!(acceptable ^ style.inCompareLexicon)) {
                            front.insert(i + 1, style.symbol);
                            i += style.symbol.length();
                        }
                    }
                }

                // Populate back hooks with symbols
                for (int i = 0; i < back.length(); ++i) {
                    QChar c = back[i];
                    QString hookWord = word + c.toUpper();

                    it.toFront();
                    while (it.hasNext()) {
                        const LexiconStyle& style = it.next();
                        bool acceptable = wordEngine->isAcceptable(
                            style.compareLexicon, hookWord);

                        if (!(acceptable ^ style.inCompareLexicon)) {
                            back.insert(i + 1, style.symbol);
                            i += style.symbol.length();
                        }
                    }
                }
            }

            query.bindValue(0, word);
            query.bindValue(1, length);
            query.bindValue(2, alphagram);
            query.bindValue(3, numUniqueLetters);
            query.bindValue(4, numVowels);
            query.bindValue(5, pointValue);
            query.bindValue(6, front.toLower());
            query.bindValue(7, back.toLower());
            query.bindValue(8, isFrontHook);
            query.bindValue(9, isBackHook);
            query.bindValue(10, symbolStr);
            query.exec();

            if ((stepNum % PROGRESS_STEP) == 0) {
                if (cancelled) {
                    transactionQuery.exec("END TRANSACTION");
                    return;
                }
                emit progress(stepNum);
            }
            ++stepNum;
        }

        // Insert words into probability table with combinations
        query.prepare("INSERT INTO probability (word, length, num_blanks, "
                      "combinations) VALUES (?, ?, ?, ?)");
        foreach (const QString& word, words) {
            for (int numBlanks = 0; numBlanks <= 2; ++numBlanks) {
                double combinations =
                    letterBag.getNumCombinations(word, numBlanks);
                query.bindValue(0, word);
                query.bindValue(1, length);
                query.bindValue(2, numBlanks);
                query.bindValue(3, combinations);
                query.exec();
            }

            if ((stepNum % PROGRESS_STEP) == 0) {
                if (cancelled) {
                    transactionQuery.exec("END TRANSACTION");
                    return;
                }
                emit progress(stepNum);
            }
            ++stepNum;
        }

        // Update number of anagrams
        query.prepare("UPDATE words SET num_anagrams=? WHERE word=?");
        foreach (const QString& word, words) {
            QString alphagram = Auxil::getAlphagram(word);
            query.bindValue(0, numAnagramsMap[alphagram]);
            query.bindValue(1, word);
            query.exec();

            if ((stepNum % PROGRESS_STEP) == 0) {
                if (cancelled) {
                    transactionQuery.exec("END TRANSACTION");
                    return;
                }
                emit progress(stepNum);
            }
            ++stepNum;
        }
    }

    transactionQuery.exec("END TRANSACTION");
}

//---------------------------------------------------------------------------
//  updateProbabilityOrder
//
//! Update probability order of words in the database.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::updateProbabilityOrder(QSqlDatabase& db, int& stepNum)
{
    QSqlQuery transactionQuery ("BEGIN TRANSACTION", db);

    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT word, num_blanks, combinations "
        "FROM probability WHERE length=? ORDER BY num_blanks ASC, "
        "combinations DESC");

    QSqlQuery updateQuery (db);
    updateQuery.prepare("UPDATE probability SET probability_order=?, "
        "min_probability_order=?, max_probability_order=? "
        "WHERE word=? and num_blanks=?");

    for (int length = 1; length <= MAX_WORD_LEN; ++length) {
        selectQuery.bindValue(0, length);
        selectQuery.exec();

        QMap<QString, QString> equalWordMap;
        int probOrder = 1;
        int minProbOrder = 1;
        double prevCombinations = 0;
        int prevNumBlanks = -1;

        while (selectQuery.next()) {
            QString word = selectQuery.value(0).toString();
            int numBlanks = selectQuery.value(1).toInt();
            double combinations = selectQuery.value(2).toDouble();

            // Entering new num blanks range, clear everything out
            if (numBlanks != prevNumBlanks) {
                int maxProbOrder = minProbOrder + equalWordMap.size() - 1;
                QMapIterator<QString, QString> it (equalWordMap);
                while (it.hasNext()) {
                    it.next();
                    QString equalWord = it.value();
                    updateQuery.bindValue(0, probOrder);
                    updateQuery.bindValue(1, minProbOrder);
                    updateQuery.bindValue(2, maxProbOrder);
                    updateQuery.bindValue(3, equalWord);
                    updateQuery.bindValue(4, prevNumBlanks);
                    updateQuery.exec();
                    ++probOrder;
                }

                equalWordMap.clear();
                probOrder = 1;
                minProbOrder = 1;
                prevCombinations = 0;
            }

            // Update probability ranges
            if ((combinations != prevCombinations) && !equalWordMap.empty()) {
                int maxProbOrder = minProbOrder + equalWordMap.size() - 1;
                QMapIterator<QString, QString> it (equalWordMap);
                while (it.hasNext()) {
                    it.next();
                    QString equalWord = it.value();

                    // Update probability order and range of this word
                    updateQuery.bindValue(0, probOrder);
                    updateQuery.bindValue(1, minProbOrder);
                    updateQuery.bindValue(2, maxProbOrder);
                    updateQuery.bindValue(3, equalWord);
                    updateQuery.bindValue(4, numBlanks);
                    updateQuery.exec();

                    if ((stepNum % PROGRESS_STEP) == 0) {
                        if (cancelled) {
                            transactionQuery.exec("END TRANSACTION");
                            return;
                        }
                        emit progress(stepNum);
                    }

                    ++probOrder;
                    ++stepNum;
                }
                minProbOrder = probOrder;
                equalWordMap.clear();
            }

            // Sort words by alphagram
            QString radix = Auxil::getAlphagram(word) + word;
            equalWordMap.insert(radix, word);

            prevCombinations = combinations;
            prevNumBlanks = numBlanks;
        }

        int maxProbOrder = minProbOrder + equalWordMap.size() - 1;
        QMapIterator<QString, QString> it (equalWordMap);
        while (it.hasNext()) {
            it.next();
            QString equalWord = it.value();
            updateQuery.bindValue(0, probOrder);
            updateQuery.bindValue(1, minProbOrder);
            updateQuery.bindValue(2, maxProbOrder);
            updateQuery.bindValue(3, equalWord);
            updateQuery.bindValue(4, prevNumBlanks);
            updateQuery.exec();
        }
    }

    transactionQuery.exec("END TRANSACTION");
}

//---------------------------------------------------------------------------
//  updateDefinitions
//
//! Update definitions of words in the database.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::updateDefinitions(QSqlDatabase& db, int& stepNum)
{
    QSqlQuery transactionQuery ("BEGIN TRANSACTION", db);

    QSqlQuery query (db);
    query.prepare("UPDATE words SET definition=? WHERE word=?");

    QMap<QString, QString> definitionMap;
    QFile definitionFile (definitionFilename);
    if (definitionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        char* buffer = new char[MAX_INPUT_LINE_LEN];
        while (definitionFile.readLine(buffer, MAX_INPUT_LINE_LEN) > 0) {
            QString line (buffer);
            line = line.simplified();
            if (!line.length() || (line.at(0) == '#')) {
                if ((stepNum % PROGRESS_STEP) == 0) {
                    if (cancelled) {
                        transactionQuery.exec("END TRANSACTION");
                        delete buffer;
                        return;
                    }
                    emit progress(stepNum);
                }
                ++stepNum;
                continue;
            }
            QString word = line.section(' ', 0, 0).toUpper();
            QString definition = line.section(' ', 1);

            query.bindValue(0, definition);
            query.bindValue(1, word);
            query.exec();

            if ((stepNum % PROGRESS_STEP) == 0) {
                if (cancelled) {
                    transactionQuery.exec("END TRANSACTION");
                    delete buffer;
                    return;
                }
                emit progress(stepNum);
            }
            ++stepNum;
        }
        delete buffer;
    }

    transactionQuery.exec("END TRANSACTION");
}

//---------------------------------------------------------------------------
//  cancel
//
//! Cancel the creation of this database.  Set the cancelled flag so the
//! operation will be stopped and the database will be deleted.
//---------------------------------------------------------------------------
void
CreateDatabaseThread::cancel()
{
    cancelled = true;
}

//---------------------------------------------------------------------------
//  cleanup
//
//! Clean up open database handles.
//---------------------------------------------------------------------------
void
CreateDatabaseThread::cleanup()
{
    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
}

//---------------------------------------------------------------------------
//  updateDefinitionLinks
//
//! Update links within definitions of words in the database.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::updateDefinitionLinks(QSqlDatabase& db, int& stepNum)
{
    getDefinitions(db, stepNum);

    if (cancelled)
        return;

    QSqlQuery updateQuery (db);
    updateQuery.prepare("UPDATE words SET definition=? WHERE word=?");

    QSqlQuery transactionQuery ("BEGIN TRANSACTION", db);

    QMapIterator<QString, QString> it (definitions);
    while (it.hasNext()) {
        it.next();
        QString word = it.key();
        QString definition = it.value();

        QStringList defs = definition.split(" / ");
        QString newDefinition;
        foreach (const QString& def, defs) {
            if (!newDefinition.isEmpty())
                newDefinition += "\n";
            newDefinition += replaceDefinitionLinks(def, MAX_DEFINITION_LINKS);
        }

        if (definition != newDefinition) {
            updateQuery.bindValue(0, newDefinition);
            updateQuery.bindValue(1, word);
            updateQuery.exec();
        }

        ++stepNum;

        if ((stepNum % PROGRESS_STEP) == 0) {
            if (cancelled) {
                transactionQuery.exec("END TRANSACTION");
                return;
            }
            emit progress(stepNum);
        }
    }

    transactionQuery.exec("END TRANSACTION");
}

//---------------------------------------------------------------------------
//  getDefinitions
//
//! Get word definitions from the database and put them in the definitions
//! map.  Discard any definitions that consist of only part of speech.
//
//! @param db the database
//! @param stepNum the current step number
//---------------------------------------------------------------------------
void
CreateDatabaseThread::getDefinitions(QSqlDatabase& db, int& stepNum)
{
    QSqlQuery selectQuery (db);
    selectQuery.prepare("SELECT word, definition FROM words");
    selectQuery.exec();

    QRegExp defRegex (QString("^[^[]|\\s+/\\s+[^[]"));

    while (selectQuery.next()) {
        QString word = selectQuery.value(0).toString();
        QString definition = selectQuery.value(1).toString();

        if (defRegex.indexIn(definition, 0) < 0) {
            ++stepNum;
            if ((stepNum % PROGRESS_STEP) == 0) {
                if (cancelled)
                    return;
                emit progress(stepNum);
            }
            continue;
        }

        definitions[word] = definition;
    }
}

//---------------------------------------------------------------------------
//  replaceDefinitionLinks
//
//! Replace links in a definition with the definitions of the words they are
//! linked to.  A string is assumed to have a maximum of one link.  Links may
//! be followed recursively to the maximum depth specified.
//
//! @param definition the definition with links to be replaced
//! @param maxDepth the maximum number of recursive links to replace
//! @param useFollow true if the "follow" replacement should be used
//
//! @return a string with links replaced
//---------------------------------------------------------------------------
QString
CreateDatabaseThread::replaceDefinitionLinks(const QString& definition, int
                                             maxDepth, bool useFollow) const
{
    QRegExp followRegex (QString("\\{(\\w+)=(\\w+)\\}"));
    QRegExp replaceRegex (QString("\\<(\\w+)=(\\w+)\\>"));

    // Try to match the follow regex and the replace regex.  If a follow regex
    // is ever matched, then the "follow" replacements should always be used,
    // even if the "replace" regex is matched in a later iteration.
    QRegExp* matchedRegex = 0;
    int index = followRegex.indexIn(definition, 0);
    if (index >= 0) {
        matchedRegex = &followRegex;
        useFollow = true;
    }
    else {
        index = replaceRegex.indexIn(definition, 0);
        matchedRegex = &replaceRegex;
    }

    if (index < 0)
        return definition;

    QString modified (definition);
    QString word = matchedRegex->cap(1);
    QString pos = matchedRegex->cap(2);

    QString replacement;
    QString upper = word.toUpper();
    QString failReplacement = useFollow ? word : upper;
    if (!maxDepth) {
        replacement = failReplacement;
    }
    else {
        QString subdef = getSubDefinition(upper, pos);
        if (subdef.isEmpty()) {
            replacement = failReplacement;
        }
        else if (useFollow) {
            replacement = (matchedRegex == &followRegex) ?
                word + " (" + subdef + ")" : subdef;
        }
        else {
            replacement = upper + ", " + subdef;
        }
    }

    modified.replace(index, matchedRegex->matchedLength(), replacement);
    int lowerMaxDepth = useFollow ? maxDepth - 1 : maxDepth;
    QString newDefinition = maxDepth
        ? replaceDefinitionLinks(modified, lowerMaxDepth, useFollow)
        : modified;
    return newDefinition;
}

//---------------------------------------------------------------------------
//  getSubDefinition
//
//! Return the definition associated with a word and a part of speech.  If
//! more than one definition is given for a part of speech, pick the first
//! one.
//
//! @param word the word
//! @param pos the part of speech
//
//! @return the definition substring
//---------------------------------------------------------------------------
QString
CreateDatabaseThread::getSubDefinition(const QString& word, const QString&
                                       pos) const
{
    if (!definitions.contains(word))
        return QString();

    QString definition = definitions[word];
    QRegExp posRegex (QString("\\[(\\w+)"));
    QStringList defs = definition.split(" / ");
    foreach (const QString& def, defs) {
        if ((posRegex.indexIn(def, 0) > 0) &&
            (posRegex.cap(1) == pos))
        {
            QString str = def.left(def.indexOf("[")).simplified();
            if (!str.isEmpty())
                return str;
        }
    }

    return QString();
}
