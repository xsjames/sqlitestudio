#ifndef COMMITTABLE_H
#define COMMITTABLE_H

#include "coreSQLiteStudio_global.h"
#include <QList>
#include <functional>

class API_EXPORT Committable
{
    public:
        typedef std::function<bool(const QList<Committable*>& instances)> ConfirmFunction;

        Committable();
        virtual ~Committable();

        virtual bool isUncommitted() const = 0;
        virtual QString getQuitUncommittedConfirmMessage() const = 0;

        static void init(ConfirmFunction confirmFunc);
        static bool canQuit();

    private:
        static ConfirmFunction confirmFunc;
        static QList<Committable*> instances;
};

#endif // COMMITTABLE_H
