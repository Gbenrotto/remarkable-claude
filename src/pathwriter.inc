// Uses the upstream evdev adapter; keep the pen down along each letter path.
QJsonObject drawLetterPaths(const std::vector<QPolygonF> &paths, const QPointF &offset, const QSize &canvas) {
    size_t total=0;
    for(const auto &path:paths)for(const auto &p:path) {
        QPointF q=p+offset;
        if(!std::isfinite(q.x()) || !std::isfinite(q.y()) || q.x()<0 || q.y()<0 || q.x()>=canvas.width() || q.y()>=canvas.height())
            return failure("outside-page","Letter path extends outside the screen");
        ++total;
    }
    if(paths.empty() || paths.size()>3000 || total>150000)
        return failure("too-complex","Answer has too many pen movements");
    if(!g_inputMutex.tryLock())return failure("busy","Another ink operation is running");
    const std::unique_ptr<QMutex,void(*)(QMutex*)> lock(&g_inputMutex,[](QMutex*m){m->unlock();});
    QString error;
    auto device=openInputDevice({},&error);
    if(!device)return failure("pen-device-unavailable",error);
    struct Lift { InputDevice &device; ~Lift(){penUp(device,nullptr);} } lift{*device};
    QElapsedTimer drawTimer;drawTimer.start();
    QElapsedTimer guardTimer;guardTimer.start();
    for(const auto &path:paths) {
        if(!diaryGuard())return failure("cancelled","Cancelled or page changed; partial ink may remain");
        if(path.isEmpty())continue;
        if(!penUp(*device,&error) || !penDownAt(*device,screenToInput(*device,path.first()+offset,canvas),&error))
            return failure("pen-write-failed",error);
        QThread::usleep(1000);
        QPointF previous=path.first();
        for(int i=1;i<path.size();++i) {
            if(guardTimer.elapsed()>50) {
                if(!diaryGuard())return failure("cancelled","Cancelled or page changed; partial ink may remain");
                guardTimer.restart();
            }
            // At most two screen pixels between reports, even for a long segment.
            int steps=std::max(1,int(std::ceil(QLineF(previous,path[i]).length()/2.0)));
            for(int step=1;step<=steps;++step) {
                QPointF p=previous+(path[i]-previous)*(double(step)/steps)+offset;
                if(!movePen(*device,screenToInput(*device,p,canvas),&error))return failure("pen-write-failed",error);
                QThread::usleep(250);
            }
            previous=path[i];
        }
        QThread::usleep(1000);
        if(!penUp(*device,&error))return failure("pen-write-failed",error);
    }
    return QJsonObject{{"ok",true},{"strokes",int(paths.size())},{"points",int(total)},{"elapsedMs",int(drawTimer.elapsed())}};
}
