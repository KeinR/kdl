(1 + 0 ? print [Hewlow world] ::
    (? print [More hwllo])
    (? print [uwu desu] ::
        (? print momomo)
        (foobar: ? print {momomo}))
    (do + 50 ? print [this should not be visible]))

