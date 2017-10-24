#include "Interpreteur.h"
#include <stdlib.h>
#include <iostream>
using namespace std;

Interpreteur::Interpreteur(ifstream & fichier) :
m_lecteur(fichier), m_table(), m_arbre(nullptr), m_nbErreurs(0) {
}

void Interpreteur::analyse() {
    m_arbre = programme(); // on lance l'analyse de la première règle
}

void Interpreteur::tester(const string & symboleAttendu) const throw (SyntaxeException) {
    // Teste si le symbole courant est égal au symboleAttendu... Si non, lève une exception
    static char messageWhat[256];
    if (m_lecteur.getSymbole() != symboleAttendu) {
        sprintf(messageWhat,
                "Ligne %d, Colonne %d - Erreur de syntaxe - Symbole attendu : %s - Symbole trouvé : %s",
                m_lecteur.getLigne(), m_lecteur.getColonne(),
                symboleAttendu.c_str(), m_lecteur.getSymbole().getChaine().c_str());
        throw SyntaxeException(messageWhat);
    }
}

void Interpreteur::testerEtAvancer(const string & symboleAttendu) throw (SyntaxeException) {
    // Teste si le symbole courant est égal au symboleAttendu... Si oui, avance, Sinon, lève une exception
    tester(symboleAttendu);
    m_lecteur.avancer();
}

void Interpreteur::erreur(const string & message) const throw (SyntaxeException) {
    // Lève une exception contenant le message et le symbole courant trouvé
    // Utilisé lorsqu'il y a plusieurs symboles attendus possibles...
    static char messageWhat[256];
    sprintf(messageWhat,
            "Ligne %d, Colonne %d - Erreur de syntaxe - %s - Symbole trouvé : %s",
            m_lecteur.getLigne(), m_lecteur.getColonne(), message.c_str(), m_lecteur.getSymbole().getChaine().c_str());
    throw SyntaxeException(messageWhat);
}

Noeud* Interpreteur::programme() {
    // <programme> ::= procedure principale() <seqInst> finproc FIN_FICHIER
    try {
        testerEtAvancer("procedure");
        testerEtAvancer("principale");
        testerEtAvancer("(");
        testerEtAvancer(")");
        Noeud* sequence = seqInst();
        testerEtAvancer("finproc");
        tester("<FINDEFICHIER>");
        return sequence;
    } catch (SyntaxeException & e) {
        cout << "SyntaxeError : " << e.what() << endl;
        m_nbErreurs++;
        return nullptr;
    }
}

Noeud* Interpreteur::seqInst() {
    // <seqInst> ::= <inst> { <inst> }
    NoeudSeqInst* sequence = new NoeudSeqInst();
    do {
        sequence->ajoute(inst());
    } while (m_lecteur.getSymbole() == "<VARIABLE>" ||
            m_lecteur.getSymbole() == "si" ||
            m_lecteur.getSymbole() == "tantque" ||
            m_lecteur.getSymbole() == "repeter" ||
            m_lecteur.getSymbole() == "pour" ||
            m_lecteur.getSymbole() == "ecrire" ||
            m_lecteur.getSymbole() == "lire");
    // Tant que le symbole courant est un début possible d'instruction...
    // Il faut compléter cette condition chaque fois qu'on rajoute une nouvelle instruction
    return sequence;
}

Noeud* Interpreteur::inst() {
    // <inst> ::= <affectation>  ; | <instSi> | <tantQue> | <instRepeter> ; | <instPour> | <ecrire>; | <lire>;
    try {
        if (m_lecteur.getSymbole() == "<VARIABLE>") {
            Noeud *affect = affectation();
            testerEtAvancer(";");
            return affect;
        } else if (m_lecteur.getSymbole() == "si")
            return instSiRiche();
        else if (m_lecteur.getSymbole() == "tantque")
            return instTantQue();
        else if (m_lecteur.getSymbole() == "pour")
            return instPour();
        else if (m_lecteur.getSymbole() == "repeter") {
            Noeud* repeter = instRepeter();
            testerEtAvancer(";");
            return repeter;
        } else if (m_lecteur.getSymbole() == "ecrire") {
            Noeud* ecrire = instEcrire();
            testerEtAvancer(";");
            return ecrire;
        } else if (m_lecteur.getSymbole() == "lire") {
            Noeud* lire = instLire();
            testerEtAvancer(";");
            return lire;
        }// Compléter les alternatives chaque fois qu'on rajoute une nouvelle instruction
        else erreur("Instruction incorrecte");
    } catch (SyntaxeException & e) {
        cout << "SyntaxeError : " << e.what() << endl;
        m_nbErreurs++;
        while (m_lecteur.getSymbole() != "<VARIABLE>" &&
                m_lecteur.getSymbole() != "si" &&
                m_lecteur.getSymbole() != "tantque" &&
                m_lecteur.getSymbole() != "repeter" &&
                m_lecteur.getSymbole() != "pour" &&
                m_lecteur.getSymbole() != "ecrire" &&
                m_lecteur.getSymbole() != "lire" &&
                m_lecteur.getSymbole() != "<FINDEFICHIER>") {
            m_lecteur.avancer();
        }
    }
}

Noeud* Interpreteur::affectation() {
    // <affectation> ::= <variable> = <expression> 
    tester("<VARIABLE>");
    Noeud* var = m_table.chercheAjoute(m_lecteur.getSymbole()); // La variable est ajoutée à la table et on la mémorise
    m_lecteur.avancer();
    testerEtAvancer("=");
    Noeud* exp = expression(); // On mémorise l'expression trouvée
    return new NoeudAffectation(var, exp); // On renvoie un noeud affectation
}

Noeud* Interpreteur::expression() {
    // <expression> ::= <facteur> { <opBinaire> <facteur> }
    //  <opBinaire> ::= + | - | *  | / | < | > | <= | >= | == | != | et | ou
    Noeud* fact = facteur();
    while (m_lecteur.getSymbole() == "+" || m_lecteur.getSymbole() == "-" ||
            m_lecteur.getSymbole() == "*" || m_lecteur.getSymbole() == "/" ||
            m_lecteur.getSymbole() == "<" || m_lecteur.getSymbole() == "<=" ||
            m_lecteur.getSymbole() == ">" || m_lecteur.getSymbole() == ">=" ||
            m_lecteur.getSymbole() == "==" || m_lecteur.getSymbole() == "!=" ||
            m_lecteur.getSymbole() == "et" || m_lecteur.getSymbole() == "ou") {
        Symbole operateur = m_lecteur.getSymbole(); // On mémorise le symbole de l'opérateur
        m_lecteur.avancer();
        Noeud* factDroit = facteur(); // On mémorise l'opérande droit
        fact = new NoeudOperateurBinaire(operateur, fact, factDroit); // Et on construuit un noeud opérateur binaire
    }
    return fact; // On renvoie fact qui pointe sur la racine de l'expression
}

Noeud* Interpreteur::facteur() {
    // <facteur> ::= <entier> | <variable> | - <facteur> | non <facteur> | ( <expression> )
    Noeud* fact = nullptr;
    if (m_lecteur.getSymbole() == "<VARIABLE>" || m_lecteur.getSymbole() == "<ENTIER>") {
        fact = m_table.chercheAjoute(m_lecteur.getSymbole()); // on ajoute la variable ou l'entier à la table
        m_lecteur.avancer();
    } else if (m_lecteur.getSymbole() == "-") { // - <facteur>
        m_lecteur.avancer();
        // on représente le moins unaire (- facteur) par une soustraction binaire (0 - facteur)
        fact = new NoeudOperateurBinaire(Symbole("-"), m_table.chercheAjoute(Symbole("0")), facteur());
    } else if (m_lecteur.getSymbole() == "non") { // non <facteur>
        m_lecteur.avancer();
        // on représente le moins unaire (- facteur) par une soustractin binaire (0 - facteur)
        fact = new NoeudOperateurBinaire(Symbole("non"), facteur(), nullptr);
    } else if (m_lecteur.getSymbole() == "(") { // expression parenthésée
        m_lecteur.avancer();
        fact = expression();
        testerEtAvancer(")");
    } else
        erreur("Facteur incorrect");
    return fact;
}

Noeud* Interpreteur::instSi() {
    // <instSi> ::= si ( <expression> ) <seqInst> finsi
    testerEtAvancer("si");
    testerEtAvancer("(");
    Noeud* condition = expression(); // On mémorise la condition
    testerEtAvancer(")");
    Noeud* sequence = seqInst(); // On mémorise la séquence d'instruction
    testerEtAvancer("finsi");
    return new NoeudInstSi(condition, sequence); // Et on renvoie un noeud Instruction Si
}

Noeud* Interpreteur::instSiRiche() {
    //<instSiRiche> ::= si (<expression>) <seqInst> { sinonsi (<expression>) <seqInst> } [sinon <seqInst>] finsi
    testerEtAvancer("si");
    testerEtAvancer("(");
    Noeud* conditionSi = expression(); // On mémorise la condition
    testerEtAvancer(")");
    Noeud* sequenceSi = seqInst(); // On mémorise la séquence d'instruction
    vector<Noeud*> vectSi;
    NoeudInstSi* si = new NoeudInstSi(conditionSi, sequenceSi);
    vectSi.push_back(si);
    Noeud* sequenceSinon = nullptr;
    while (m_lecteur.getSymbole() == "sinonsi") {
        m_lecteur.avancer();
        testerEtAvancer("(");
        Noeud* conditionSinonSi = expression(); // On mémorise la condition
        testerEtAvancer(")");
        Noeud* sequenceSinonSi = seqInst(); // On mémorise la séquence d'instruction
        NoeudInstSi* sinonSi = new NoeudInstSi(conditionSinonSi, sequenceSinonSi);
        vectSi.push_back(sinonSi);
    }
    if (m_lecteur.getSymbole() == "sinon") {
        m_lecteur.avancer();
        sequenceSinon = seqInst();
    }
    testerEtAvancer("finsi");
    return new NoeudInstSiRiche(vectSi, sequenceSinon);
}

Noeud* Interpreteur::instTantQue() {
    // <instTantQue> ::= tantque ( <expression> ) <seqInst> fintantque
    testerEtAvancer("tantque");
    testerEtAvancer("(");
    Noeud* condition = expression(); // On mémorise la condition
    testerEtAvancer(")");
    Noeud* sequence = seqInst();
    testerEtAvancer("fintantque");
    return new NoeudInstTantQue(condition, sequence);
}

Noeud* Interpreteur::instRepeter() {
    // <instRepeter> ::= repeter <seqInst> jusqua ( <expression> )
    testerEtAvancer("repeter");
    Noeud* instruction = seqInst();
    testerEtAvancer("jusqua");
    testerEtAvancer("(");
    Noeud* exp = expression();
    testerEtAvancer(")");
    return new NoeudInstRepeter(instruction, exp);
}

Noeud* Interpreteur::instPour() {
    //   <instPour>::= pour( [ <affectation> ] ; <expression> ;[ <affectation> ]) <seqInst> finpour
    testerEtAvancer("pour");
    testerEtAvancer("(");
    Noeud* init = nullptr;
    if (m_lecteur.getSymbole() == "<VARIABLE>") {
        init = affectation();
    }
    testerEtAvancer(";");
    tester("<VARIABLE>");
    Noeud* cond = expression();
    testerEtAvancer(";");
    Noeud* incr = nullptr;
    if (m_lecteur.getSymbole() == "<VARIABLE>") {
        incr = affectation();
    }
    testerEtAvancer(")");
    Noeud* sequence = seqInst();
    testerEtAvancer("finpour");
    return new NoeudInstPour(init, cond, incr, sequence);
}

Noeud* Interpreteur::instEcrire() {
    // <instEcrire> ::= ecrire( <expression> | <chaine> {, <expression> | <chaine> })
    testerEtAvancer("ecrire");
    testerEtAvancer("(");
    NoeudEcrire* ecrire = new NoeudEcrire();
    if (m_lecteur.getSymbole() == "<CHAINE>") {
        ecrire->ajoute(m_table.chercheAjoute(m_lecteur.getSymbole()));
        m_lecteur.avancer();
    } else
        ecrire->ajoute(expression());

    while (m_lecteur.getSymbole() == ",") {
        testerEtAvancer(",");
        if (m_lecteur.getSymbole() == "<CHAINE>") {
            ecrire->ajoute(m_table.chercheAjoute(m_lecteur.getSymbole()));
            m_lecteur.avancer();
        } else
            ecrire->ajoute(expression());
    }
    testerEtAvancer(")");
    return ecrire;
}

Noeud* Interpreteur::instLire() {
    //   <instLire> ::= lire ( <variable> { , <variable> } ) 
    testerEtAvancer("lire");
    testerEtAvancer("(");

    NoeudLire* lire = new NoeudLire();
    lire->ajoute(m_table.chercheAjoute(m_lecteur.getSymbole()));
    m_lecteur.avancer();

    while (m_lecteur.getSymbole() == ",") {
        testerEtAvancer(",");
        lire->ajoute(m_table.chercheAjoute(m_lecteur.getSymbole()));
        m_lecteur.avancer();
    }
    testerEtAvancer(")");
    return lire;
}