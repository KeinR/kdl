
(akatsuki: {>armor health} < 0.2 , enemies > 2 ? ::
    (? do [travel home port] ::
        (atHome ? do [dock]))
    (akagi: ? do [join akatsuki]))

(akatsuki: speed > 3 ? do [activate radar])

(akatsuki: enemies > 5 ? ::
    (akagi: closeToAkatsuki, {akatsuki enemies} > 7 ? do [send air support]))



